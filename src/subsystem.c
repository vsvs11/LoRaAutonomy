#include <stdio.h>
#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "subsystems.h"
#include "esp_attr.h"
#include "list.h"
#include "drivers/ili9341.h"
#include "drivers/sx127x.h"

static const char *TAG = "POWER_LOG";

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
    uint8_t packet[] = {0x01, 0x13, 0x34, 0xAB, 0x04};
    for (;;){
         ESP_LOGI(TAG, "SEND");
        lora_send_packet(packet, sizeof(packet));
         ESP_LOGI(TAG, "OK");
        vTaskDelay(pdMS_TO_TICKS(1000));

    }

}
