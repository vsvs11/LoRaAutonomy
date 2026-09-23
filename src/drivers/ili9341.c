#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "driver/gpio.h"        
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


#define MOSI_NUM 11
#define SCLK_NUM 12
#define RESET_NUM 5
#define DC_NUM 4
#define CS_NUM 18
#define MAX_TRANSFER_SZ 15000

static spi_device_handle_t spi_dev;
uint8_t cmd;

void display_cmd(uint8_t cmd) {
    gpio_set_level(DC_NUM, 0); 

    spi_transaction_t t = {
        .length = 8,        
        .tx_buffer = &cmd   
    };
    spi_device_polling_transmit(spi_dev, &t);
}

void display_data_byte(uint8_t data) {
    gpio_set_level(DC_NUM, 1); 
    spi_transaction_t t = {
        .flags = SPI_TRANS_USE_TXDATA,
        .length = 8,
        .tx_data = { data }
    };
    spi_device_polling_transmit(spi_dev, &t);
}

void display_data(void *data, size_t len){
    gpio_set_level(DC_NUM, 1); 

    spi_transaction_t t_data = {
        .length = len * 8, 
        .tx_buffer = data
    };
    spi_device_polling_transmit(spi_dev, &t_data);
}

void display_init(void){
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << DC_NUM) | (1ULL << RESET_NUM),
        .mode = GPIO_MODE_OUTPUT,            
        .pull_up_en = GPIO_PULLUP_DISABLE,     
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE       
    };
    gpio_config(&gpio_conf);
    gpio_set_level(DC_NUM,0);
    gpio_set_level(RESET_NUM,1);

    spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI_NUM,
        .sclk_io_num = SCLK_NUM,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 15000
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 80000000,
        .mode = 0,
        .spics_io_num = CS_NUM,
        .queue_size = 7
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &spi_dev);
    
    display_cmd(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    display_cmd(0x3A);
    display_data_byte(0x55);
    display_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
    
}
// Вспомогательная функция установки окна на весь дисплей
void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t caset[4] = { (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF) };
    uint8_t raset[4] = { (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF) };

    display_cmd(0x2A); // CASET (колонки)
    display_data(caset, 4);

    display_cmd(0x2B); // RASET (строки)
    display_data(raset, 4);

    display_cmd(0x2C); // RAMWR (старт записи в память)
}

// Функция заливки всего экрана одним цветом
void display_fill_screen(uint16_t color) {
    // 1. Открываем окно на весь экран: 320x240
    display_set_window(0, 0, 319, 239);

    // 2. Буфер на 20 строк: 320 * 20 = 6400 пикселей = 12800 байт
    const size_t chunk_pixels = 320 * 20;
    const size_t chunk_bytes = chunk_pixels * sizeof(uint16_t);

    uint16_t *line_buf = (uint16_t *)heap_caps_malloc(chunk_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!line_buf) {
        printf("Не удалось выделить память под буфер!\n");
        return;
    }

    // 3. Заполняем буфер цветом (с учетом Endianness для SPI: свапаем байты)
    uint16_t swapped_color = (color >> 8) | (color << 8);
    for (size_t i = 0; i < chunk_pixels; i++) {
        line_buf[i] = swapped_color;
    }

    // 4. Проталкиваем буфер в контроллер 12 раз (12 * 20 строк = 240 строк)
    for (int i = 0; i < 12; i++) {
        display_data(line_buf, chunk_bytes);
    }

    // 5. Освобождаем память (или держи буфер статическим/глобальным, если часто заливаешь)
    free(line_buf);
}

