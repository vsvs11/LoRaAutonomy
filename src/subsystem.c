#include <stdint.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "subsystems.h"
#include "esp_attr.h"
#include "list.h"
#include "drivers/ili9341.h"
#include "drivers/sx127x.h"
#include "subsystems/lora_tdma/lora_packet.h"
#include "drivers/atgm336h.h"
#include "esp_timer.h"
#include "driver/uart.h"

// Глобальная метка секундного фронта
static volatile int64_t g_last_pps_us = 0;

static const char *TAG = "POWER_LOG";
static const char *GPS_TAG = "GPS_NODE";

void hard(void *pvParameters){
    for (;;){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void power(void *pvParameters) {
    (void)pvParameters; // Глушим варнинг о неиспользуемом аргументе

    for (;;) {
        // Забираем УКАЗАТЕЛЬ на массив
        list_tasks_t *task_list = list_get();

        ESP_LOGI(TAG, "--- Active Tasks Dump ---");

        for (int i = 0; i < TASKS_MAX; i++) {
            // Проверяем, что слот реально занят таской, чтобы не читать пустую память
            if (task_list[i].name != NULL && task_list[i].handle != NULL) {
                ESP_LOGI(TAG, "[Slot %d] Name: %s | Func: %p | Stack: %lu | Prio: %u",
                         i,
                         task_list[i].name,
                         (void *)task_list[i].func,
                         (unsigned long)task_list[i].stack_bytes,
                         (unsigned int)task_list[i].priority);
            }
        }

        // Задержка на весь дамп: спим 2 секунды перед следующим выводом
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void display(void *pvParameters){
    display_init();
    display_fill_screen(0x0000);
    display_fill_rect(20,50,140,170,0xFFFF);
    display_fill_rect(21,51,139,169,0x0000);
    display_fill_circle(250,100,50,0xF800);
    display_draw_line(20,210,200,20,0x07E0);
    display_draw_string(100,200,"Hello,World!",0xFFFF,0x0000, 2);
    for (;;){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void lora(void *pvParameters){
    lora_config_t conf = {
    .bw_idx = 7,             // 7 соответствует полосе 125 кГц
    .cr_idx = 5,             // 5 соответствует кодированию 4/5 (библиотека сама вычтет 4)
    .crc_on = true,          // Включит CRC (в коде сработает условие 1 << 2)
    .freq = 433,             // Частота 433 МГц (в Герцах)
    .sf = 7                  // Коэффициент SF7 (оптимально для тестов)
    };
    lora_init(&conf);
    int ver = lora_reg_read(0x42);
    if (ver != 0x12){
        for (;;){
            ESP_LOGI(TAG, "Error read register");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

    // 1. Выделяем сплошной кусок памяти: Заголовок + Данные
    // tx_buffer физически занимает 8 + 10 = 18 байт
    uint8_t tx_buffer[sizeof(packet_handle_t) + sizeof(data)];

    // 2. Накладываем "трафарет" нашей структуры на начало буфера
    packet_handle_t *paket = (packet_handle_t *)tx_buffer;

    // 3. Заполняем заголовок
    paket->id_recipient = 0x02;
    paket->id_sender = 0x01;
    paket->type_paket = 0x01;
    paket->flags = 0xFFFF;
    paket->num_packet = 0;
    paket->crc = 0xFF; // Заглушка, по-хорошему тут вызов calc_crc8()
    paket->payload_len = sizeof(data);

    // 4. Физически копируем данные в хвост структуры (в flexible array)
    memcpy(paket->payload, data, sizeof(data));

    // Полный размр пакета для отправки (8 байт заголовка + 10 байт данных)
    size_t total_tx_size = sizeof(packet_handle_t) + paket->payload_len;
    for (;;){
         ESP_LOGI(TAG, "SEND");
        lora_send_packet(tx_buffer,total_tx_size);
         ESP_LOGI(TAG, "OK");
        vTaskDelay(pdMS_TO_TICKS(1000));

    }
}

/*
 * GNSS and timing manager task for LoRaAutonomy node
 */
void gps_manager_task(void *pvParameters) {
    (void)pvParameters;

    /*
     * Initialization ATGM336H driver with 115200 baud and PPS input
     */
    atgm_config_t cfg = {
        .baud_rate      = 115200,
        .nav_mode       = ATGM_MODE_ALL,
        .update_rate_hz = 1,
        .pps_gpio       = ATGM_PPS_NUM,
        .pps_led_gpio   = -1 // Set to GPIO num if external LED is wired
    };

    atgm_init(&cfg);
    ESP_LOGI(GPS_TAG, "ATGM336H driver started. Waiting for PPS and GNSS lock...");

    atgm_data_t nav;
    atgm_pps_t  pps;
    int64_t edge_us = 0;
    uint64_t synced_unix_us = 0;

    for (;;) {
        /*
         * Wait for hardware PPS edge (1.5s timeout for fallback)
         */
        bool pps_received = atgm_wait_pps(1500, &edge_us);
        if (pps_received) {
            g_last_pps_us = edge_us;
        }

        /*
         * Query latest navigation and timing state
         */
        bool has_fix = atgm_get_data(&nav);
        atgm_get_pps(&pps);
        atgm_get_synced_unix_us(&synced_unix_us);

        /*
         * Format and log full node telemetry
         */
        printf("\n================ [LoRaAutonomy NODE STATUS] ================\n");
        printf(" Fix Status   : %s (Mode: %uD, Quality: %u)\n",
               has_fix ? "3D-FIX LOCKED" : "SEARCHING",
               nav.fix_dimension,
               nav.fix_quality);
        printf(" Coordinates  : %.6f° N, %.6f° E\n", nav.latitude, nav.longitude);
        printf(" Scaled Coords: Lat=%" PRId32 ", Lon=%" PRId32 " (x1e6 for LoRa)\n",
               nav.lat_scaled, nav.lon_scaled);
        printf(" Altitude     : %.1f m (Alt cm: %ld, Geoid: %.1f m)\n",
               nav.altitude, (long)nav.alt_cm, nav.geoid_sep);
        printf(" Speed/Course : %.2f m/s | Track: %.1f°\n",
               nav.speed_mps, nav.course_deg);
        printf(" Satellites   : %u used | In View: GPS=%u, BDS=%u, GLO=%u | Peak SNR: %u dB-Hz\n",
               nav.sats_used, nav.sats_view_gps, nav.sats_view_bds, nav.sats_view_glo, nav.max_snr);
        printf(" Precision    : HDOP=%.2f (Q8.8=0x%04X), PDOP=%.2f, VDOP=%.2f\n",
               nav.hdop, nav.hdop_q8_8, nav.pdop, nav.vdop);
        printf(" UTC Time     : %04u-%02u-%02u %02u:%02u:%02u.%03u (Raw: %06lu / %06lu)\n",
               nav.utc.year, nav.utc.month, nav.utc.day,
               nav.utc.hour, nav.utc.minute, nav.utc.second, nav.utc.millisecond,
               (unsigned long)nav.raw_time, (unsigned long)nav.raw_date);
        printf(" Synced Unix  : %" PRIu64 " us (Epoch sec: %" PRIu64 ")\n",
               synced_unix_us, nav.unix_epoch);
        printf(" PPS Health   : Period=%" PRId32 " us | Jitter=%" PRId32 " us | Drift=%.2f PPM | Locked=%d\n",
               pps.period_us, pps.jitter_us, pps.drift_ppm, pps.locked);
        printf(" Antenna      : %s\n",
               nav.antenna_status == ATGM_ANT_OK ? "OK" :
               nav.antenna_status == ATGM_ANT_OPEN ? "OPEN CIRCUIT" :
               nav.antenna_status == ATGM_ANT_SHORT ? "SHORT CIRCUIT" : "UNKNOWN");
        printf(" Stats        : Sentences=%lu, Checksum Errors=%lu\n",
               (unsigned long)nav.rx_sentences, (unsigned long)nav.crc_errors);
        printf("============================================================\n");
    }
}