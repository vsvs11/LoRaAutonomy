#include "esp_timer.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_attr.h"
#include "drivers/sx127x.h"
#include "subsystems/lora_tdma/tdma.h"
#include "subsystems/lora_tdma/lora_packet.h"

#define DIO_NUM          2             // Пин DIO0 от SX127x (согласно config.h / распиновке)
#define TIME_SLOT_US     500000ULL     // Длительность одного слота: 500 мс (0.5 сек)
#define NUM_NODES        3             // Всего нод в тестовой сети
#define MY_NODE_ID       1             // ID этой ноды (поменяй на 0 для второй платы)

#define EVT_SLOT_TICK    (1UL << 0)
#define EVT_DIO0_IRQ     (1UL << 1)

static const char *TAG = "TDMA_LOG";
static TaskHandle_t s_tdma_task_hdl = NULL;
static esp_timer_handle_t s_slot_timer = NULL;
static volatile uint32_t s_current_slot = 0;

// Аппаратное прерывание от DIO0 (выполняется в IRAM за пару микросекунд)
static void IRAM_ATTR gpio_isr_handler(void *arg){
    (void)arg;
    BaseType_t woken = pdFALSE;
    if (s_tdma_task_hdl != NULL) {
        xTaskNotifyFromISR(s_tdma_task_hdl, EVT_DIO0_IRQ, eSetBits, &woken);
        if (woken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

// Колбэк аппаратного таймера слотов
static void slot_timer_callback(void *arg) {
    (void)arg;
    if (s_tdma_task_hdl != NULL) {
        xTaskNotify(s_tdma_task_hdl, EVT_SLOT_TICK, eSetBits);
    }
}

void tdma(void *pvParameters){
    (void)pvParameters;

    // 1. Инициализация пина DIO0 на прерывание по переднему фронту (0 -> 1)
    gpio_config_t g_conf = {
        .intr_type = GPIO_INTR_POSEDGE,
        .pin_bit_mask = (1ULL << DIO_NUM),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&g_conf);
    gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    gpio_isr_handler_add(DIO_NUM, gpio_isr_handler, NULL);

    // 2. Инициализация радиомодуля LoRa
    lora_config_t conf = {
        .bw_idx = 7,             // 125 кГц[cite: 2]
        .cr_idx = 5,             // 4/5[cite: 2]
        .crc_on = true,          // CRC включен[cite: 2]
        .freq = 433,             // 433 МГц[cite: 2]
        .sf = 7                  // SF7[cite: 2]
    };
    lora_init(&conf);

    int ver = lora_reg_read(0x42);
    if (ver != 0x12){
        for (;;){
            ESP_LOGI(TAG, "Error read register VER=0x%02x", ver);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // Сохраняем хэндл задачи для прерываний
    s_tdma_task_hdl = xTaskGetCurrentTaskHandle();

    // 3. Запускаем периодический таймер слотов
    const esp_timer_create_args_t timer_args = {
        .callback = &slot_timer_callback,
        .name = "tdma_timer"
    };
    esp_timer_create(&timer_args, &s_slot_timer);
    esp_timer_start_periodic(s_slot_timer, TIME_SLOT_US);

    // 4. Подготавливаем тестовый пакет с твоей структурой packet_handle_t[cite: 2]
    uint8_t data[] = {0xDE, 0xAD, 0xBE, 0xEF};
    uint8_t tx_buffer[sizeof(packet_handle_t) + sizeof(data)];
    packet_handle_t *paket = (packet_handle_t *)tx_buffer;
    paket->id_recipient = 0xFF; // Всем
    paket->id_sender    = MY_NODE_ID;
    paket->type_paket   = 0x01;
    paket->flags        = 0xFFFF;
    paket->num_packet   = 0;
    paket->crc          = 0x00; 
    paket->payload_len  = sizeof(data);
    memcpy(paket->payload, data, sizeof(data));
    size_t total_tx_size = sizeof(packet_handle_t) + paket->payload_len;

    uint8_t rx_message[256];
    uint32_t notifications = 0;

    // Переводим рацию в непрерывный приём при старте
    lora_start_rx_continuous();
    ESP_LOGI(TAG, "TDMA запущен. Node ID = %d, Мой слот TX = %d", MY_NODE_ID, MY_NODE_ID % NUM_NODES);

    for (;;){
        // Ждем событие от таймера слота или прерывания DIO0
        xTaskNotifyWait(0x00, 0xFFFFFFFFUL, &notifications, portMAX_DELAY);

        // --- Обработка прерывания от чипа LoRa (DIO0) ---
        if (notifications & EVT_DIO0_IRQ) {
            int guard = 5;
            while (gpio_get_level(DIO_NUM) == 1 && (guard-- > 0)) {
                uint8_t irq = lora_reg_read(0x12);
                if (irq == 0) break;

                ESP_LOGI(TAG, "[DIO0] Сработал IRQ(0x12)=0x%02x", irq);

                // 1. Если завершилась наша передача (TxDone = 0x08)
                if (irq & IRQ_TX_DONE_MASK) {
                    lora_reg_write(0x12, IRQ_TX_DONE_MASK); // Сбрасываем только флаг TX
                    ESP_LOGI(TAG, "[DIO0] Пакет успешно улетел в эфир! Возврат в RX.");
                    lora_start_rx_continuous();
                }

                // 2. Если принят пакет из эфира (RxDone = 0x40)
                if (irq & IRQ_RX_DONE_MASK) {
                    // НЕ сбрасываем 0x12 здесь — его сама проверит и сбросит lora_receive_packet()!
                    int len = lora_receive_packet(rx_message, sizeof(rx_message));
                    if (len > 0) {
                        packet_handle_t *incoming = (packet_handle_t *)rx_message;
                        ESP_LOGI(TAG, ">>> [DIO0] ПРИНЯТ ПАКЕТ от Node 0x%02x | №%u | длина %d байт | RSSI=%d dBm",
                                 incoming->id_sender, incoming->num_packet, len, lora_packet_rssi());

                        // Автосинхронизация слотов по эфиру: подстраиваемся под Node 0
                        if (MY_NODE_ID != 0 && incoming->id_sender == 0) {
                            s_current_slot = 1; // Следующий слот после Node 0 — это слот 1
                            esp_timer_stop(s_slot_timer);
                            esp_timer_start_periodic(s_slot_timer, TIME_SLOT_US);
                        }
                    } else if (len == -1) {
                        ESP_LOGW(TAG, "[DIO0] Ошибка CRC, пакет отброшен.");
                    }
                }

                // На случай любых других побочных флагов (например, ValidHeader 0x10)
                uint8_t rem = lora_reg_read(0x12);
                if (rem != 0) {
                    lora_reg_write(0x12, rem);
                }
            }
        }

        // --- Обработка смены тайм-слота ---
        if (notifications & EVT_SLOT_TICK) {
            // Рассчитываем номер текущего слота на основе глобального счётчика
            uint32_t active_nod = s_current_slot % NUM_NODES;

            if (active_nod == MY_NODE_ID) {
                // Наступил наш слот — отправляем пакет асинхронно
                ESP_LOGI(TAG, "--- Слот %lu (Мой TX) ---", (unsigned long)s_current_slot);
                paket->num_packet++;
                lora_send_packet_async(tx_buffer, total_tx_size);
            } else {
                // Чужой слот — убеждаемся, что мы в режиме приёма
                uint8_t opmode = lora_reg_read(0x01);
                if (opmode != 0x85) { // 0x85 = LoRa + RX_CONTINUOUS
                    lora_start_rx_continuous();
                }
            }

            s_current_slot++;
        }
    }
}