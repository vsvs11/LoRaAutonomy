#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "subsystems.h"
#include "esp_attr.h"
#include "list.h"



static const char *TAG = "";

void power(void *pvParameters){
    for (;;){
        struct list_tasks *list = list_get();
        ESP_LOGI(TAG, "%s\n",list->name_task);
        ESP_LOGI(TAG, "%x\n",list->pointer);
        ESP_LOGI(TAG, "%d\n",list->steck_depth);
        ESP_LOGI(TAG, "%d\n",list->priority);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
void hard(void *pvParameters){
    ESP_LOGI(TAG, "start Task interrupt_button");
}


//pdMS_TO_TICKS(1000)