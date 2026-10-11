#include "esp_timer.h"
#include <stdint.h>
#include <stdio.h>
#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "subsystems/lora_tdma/tdma.h"

#define TIME_SLOT   1000000
#define NUM_NODES   3
#define ID          0x01

static const char *TAG = "TDMA_LOG";



void tdma(void *pvParameters){
    for (;;){
        if (esp_timer_get_time() > 1000){
            int64_t start_time = esp_timer_get_time();
            int32_t slot = (uint32_t)(start_time / TIME_SLOT);
            int32_t nod = slot % NUM_NODES;

            ESP_LOGI(TAG, "slot - %d", slot);
            ESP_LOGI(TAG, "id - 0x%x", nod);
            if (nod == ID){
                ESP_LOGI(TAG, "send");
            }else{
                ESP_LOGI(TAG, "sleep");
            }
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}