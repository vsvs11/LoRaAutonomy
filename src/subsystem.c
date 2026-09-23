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

    const size_t chunk_pixels = 320 * 20; // 6400 пикселей
    const size_t chunk_bytes = chunk_pixels * sizeof(uint16_t); // 12800 байт

    uint16_t *pixels = (uint16_t *)heap_caps_malloc(chunk_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!pixels) {
        printf("Ошибка DMA malloc!\n");
        vTaskDelete(NULL);
    }

    for (;;) {
        // --- Заливаем экран КРАСНЫМ ---
        // 0xF800 в RGB565 со свапом байт под SPI -> 0x00F8
        for (int i = 0; i < chunk_pixels; i++) {
            pixels[i] = 0x00F8; 
        }

        display_set_window(0, 0, 239, 319);
        for (int i = 0; i < 12; i++) {
            display_data(pixels, chunk_bytes);
        }

        vTaskDelay(pdMS_TO_TICKS(500));

        // --- Заливаем экран СИНИМ ---
        // 0x001F в RGB565 со свапом байт -> 0x1F00
        for (int i = 0; i < chunk_pixels; i++) {
            pixels[i] = 0x1F00;
        }

        display_set_window(0, 0, 239, 319);
        for (int i = 0; i < 12; i++) {
            display_data(pixels, chunk_bytes);
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}