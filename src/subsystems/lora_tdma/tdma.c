#include "esp_timer.h"
#include <stdint.h>
#include <stdio.h>
#include "stdint.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "drivers/sx127x.h"
#include "subsystems/lora_tdma/tdma.h"
#include "subsystems/lora_tdma/lora_packet.h"

#define TIME_SLOT   1000000
#define NUM_NODES   3
#define ID          0x01

static const char *TAG = "TDMA_LOG";
uint8_t data[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
int flag = 0;
uint8_t message[256];


void tdma(void *pvParameters){
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
    
    uint8_t tx_buffer[sizeof(packet_handle_t) + sizeof(data)];
    packet_handle_t *paket = (packet_handle_t *)tx_buffer;
    paket->id_recipient = 0x02;
    paket->id_sender = 0x01;
    paket->type_paket = 0x01;
    paket->flags = 0xFFFF;
    paket->num_packet = 0;
    paket->crc = 0xFF; 
    paket->payload_len = sizeof(data);
    memcpy(paket->payload, data, sizeof(data));
    size_t total_tx_size = sizeof(packet_handle_t) + paket->payload_len;
    for (;;){
        if (esp_timer_get_time() > 1000){
            int64_t start_time = esp_timer_get_time();
            int32_t slot = (uint32_t)(start_time / TIME_SLOT);
            int32_t nod = slot % NUM_NODES;

            ESP_LOGI(TAG, "slot - %d", slot);
            ESP_LOGI(TAG, "id - 0x%x", nod);
            if (nod == ID){
                ESP_LOGI(TAG, "send");
                if (flag <= 0){
                    lora_send_packet(tx_buffer,total_tx_size);
                    ESP_LOGI(TAG, "OK SEND");
                    flag++;
                }
            }else{
                ESP_LOGI(TAG, "sleep");
                flag = 0;
                int len = lora_receive_packet(message,sizeof(message));
                if (len > 0){
                    ESP_LOGI(TAG, "OK");
                    for (int i = 0; i < 5; i++){
                        ESP_LOGI(TAG, "%x", message[i]);
                    }
                }
            }
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}