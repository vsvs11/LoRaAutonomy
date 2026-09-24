#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "subsystems.h"
#include "esp_attr.h"
#include "list.h"
#include "drivers/ili9341.h"

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