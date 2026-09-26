#include "driver/gpio.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define DS18B20_PIN  GPIO_NUM_4

static portMUX_TYPE ds_mux = portMUX_INITIALIZER_UNLOCKED;


bool ds18b20_reset(void){
    bool presence = false;
    
    portENTER_CRITICAL(&ds_mux);
    gpio_set_level(DS18B20_PIN, 0);       
    portEXIT_CRITICAL(&ds_mux);
    esp_rom_delay_us(480);                

    portENTER_CRITICAL(&ds_mux);
    gpio_set_level(DS18B20_PIN, 1);      
    esp_rom_delay_us(70);                 
    presence = (gpio_get_level(DS18B20_PIN) == 0);
    portEXIT_CRITICAL(&ds_mux);

    esp_rom_delay_us(410);             
    return presence;
}


void ds18b20_write_bit(uint8_t bit){
    portENTER_CRITICAL(&ds_mux);
    gpio_set_level(DS18B20_PIN, 0);
    if (bit) {
        esp_rom_delay_us(6);             
        gpio_set_level(DS18B20_PIN, 1);
        portEXIT_CRITICAL(&ds_mux);
        esp_rom_delay_us(64);
    } else {
        esp_rom_delay_us(60);            
        gpio_set_level(DS18B20_PIN, 1);
        portEXIT_CRITICAL(&ds_mux);
        esp_rom_delay_us(10);
    }
}


uint8_t ds18b20_read_bit(void){
    uint8_t bit = 0;
    portENTER_CRITICAL(&ds_mux);
    gpio_set_level(DS18B20_PIN, 0);      
    esp_rom_delay_us(2);
    gpio_set_level(DS18B20_PIN, 1);       
    esp_rom_delay_us(10);                
    bit = gpio_get_level(DS18B20_PIN);
    portEXIT_CRITICAL(&ds_mux);
    esp_rom_delay_us(55);
    return bit;
}


void ds18b20_write_byte(uint8_t data){
    for (int i = 0; i < 8; i++) {
        ds18b20_write_bit(data & 0x01);
        data >>= 1;
    }
}


uint8_t ds18b20_read_byte(void){
    uint8_t data = 0;
    for (int i = 0; i < 8; i++) {
        if (ds18b20_read_bit()) {
            data |= (1 << i);
        }
    }
    return data;
}

float ds18b20_read_temp(void){
    if (!ds18b20_reset()) {
        return -999.0f; 
    }
    ds18b20_write_byte(0xCC);
    ds18b20_write_byte(0x44);

    vTaskDelay(pdMS_TO_TICKS(750));

    if (!ds18b20_reset()) {
        return -999.0f;
    }
    ds18b20_write_byte(0xCC); // Skip ROM
    ds18b20_write_byte(0xBE); // Read Scratchpad

    uint8_t lsb = ds18b20_read_byte();
    uint8_t msb = ds18b20_read_byte();
    int16_t raw_temp = (int16_t)((msb << 8) | lsb);

    return (float)raw_temp * 0.0625f;
}

int ds18b20_init(void){
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << DS18B20_PIN),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD, 
        .pull_up_en = GPIO_PULLUP_ENABLE, 
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE   
    };
    gpio_config(&gpio_conf);
    gpio_set_level(DS18B20_PIN, 1);

}