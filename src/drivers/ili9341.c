#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "driver/gpio.h"        
#include "driver/spi_master.h"
#include "esp_heap_caps.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "font5x7.h"


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
    spi_device_transmit(spi_dev, &t_data);
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
    display_cmd(0x36);              // MADCTL
    display_data_byte(0x28);        // Альбомная ориентация (MV = 1) + BGR
    vTaskDelay(pdMS_TO_TICKS(120));
    display_cmd(0x3A);
    display_data_byte(0x55);
    display_cmd(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));
    
}

void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t caset[4] = { (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF), (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF) };
    uint8_t raset[4] = { (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF), (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF) };

    display_cmd(0x2A); 
    display_data(caset, 4);

    display_cmd(0x2B); 
    display_data(raset, 4);

    display_cmd(0x2C); 
}


void display_fill_screen(uint16_t color) {
    display_set_window(0, 0, 319, 239);

    const size_t chunk_pixels = 320 * 20;
    const size_t chunk_bytes = chunk_pixels * sizeof(uint16_t);

    uint16_t *line_buf = (uint16_t *)heap_caps_malloc(chunk_bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!line_buf) {
        printf("Error DMA memory!\n");
        return;
    }

    uint16_t swapped_color = (color >> 8) | (color << 8);
    for (size_t i = 0; i < chunk_pixels; i++) {
        line_buf[i] = swapped_color;
    }

    for (int i = 0; i < 12; i++) {
        display_data(line_buf, chunk_bytes);
    }

    free(line_buf);
}

void display_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) {
    if (x0 > x1 || y0 > y1) return;

    display_set_window(x0, y0, x1, y1);

    size_t w = (size_t)(x1 - x0 + 1);
    size_t h = (size_t)(y1 - y0 + 1);
    size_t total_pixels = w * h;

    #define CHUNK_PIXELS 64
    static uint16_t chunk[CHUNK_PIXELS];

    uint16_t swapped_color = (color >> 8) | (color << 8);
    for (size_t i = 0; i < CHUNK_PIXELS; i++) {
        chunk[i] = swapped_color;
    }

    while (total_pixels > 0) {
        size_t to_send = (total_pixels > CHUNK_PIXELS) ? CHUNK_PIXELS : total_pixels;
        display_data(chunk, to_send * sizeof(uint16_t));
        total_pixels -= to_send;
    }
}

void display_fill_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color) {
    int16_t x = 0;
    int16_t y = r;
    int16_t d = 3 - 2 * r;

    while (y >= x) {
        // Горизонтальные линии между симметричными точками
        display_fill_rect(x0 - x, y0 + y, x0 + x, y0 + y, color);
        display_fill_rect(x0 - x, y0 - y, x0 + x, y0 - y, color);
        display_fill_rect(x0 - y, y0 + x, x0 + y, y0 + x, color);
        display_fill_rect(x0 - y, y0 - x, x0 + y, y0 - x, color);

        if (d < 0) {
            d += 4 * x + 6;
        } else {
            d += 4 * (x - y) + 10;
            y--;
        }
        x++;
    }

}

void display_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    // Строгие горизонтальные и вертикальные линии выгоднее лить сразу пачкой
    if (y0 == y1) {
        if (x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }
        display_fill_rect(x0, y0, x1, y0, color);
        return;
    }
    if (x0 == x1) {
        if (y0 > y1) { int16_t t = y0; y0 = y1; y1 = t; }
        display_fill_rect(x0, y0, x0, y1, color);
        return;
    }

    // Алгоритм Брезенхема для произвольного наклона
    int16_t dx = abs(x1 - x0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = -abs(y1 - y0);
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx + dy;

    for (;;) {
        // Ставим один пиксель
        display_fill_rect(x0, y0, x0, y0, color);

        if (x0 == x1 && y0 == y1) break;

        int16_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void display_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size) {
   
    const uint8_t *bitmap = &font[(uint8_t)c * 5];

    
    for (int8_t i = 0; i < 5; i++) {
        uint8_t line = bitmap[i]; 

        
        for (int8_t j = 0; j < 8; j++) {
            if (line & 0x01) {
               
                if (size == 1) {
                    display_fill_rect(x + i, y + j, x + i, y + j, color);
                } else {
                    display_fill_rect(x + i * size, y + j * size, x + (i + 1) * size - 1, y + (j + 1) * size - 1, color);
                }
            } else if (bg != color) {
                
                if (size == 1) {
                    display_fill_rect(x + i, y + j, x + i, y + j, bg);
                } else {
                    display_fill_rect(x + i * size, y + j * size, x + (i + 1) * size - 1, y + (j + 1) * size - 1, bg);
                }
            }
            line >>= 1;
        }
    }

   
    if (bg != color) {
        display_fill_rect(x + 5 * size, y, x + 6 * size - 1, y + 8 * size - 1, bg);
    }
}
void display_draw_string(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size) {
    int16_t cursor_x = x;
    int16_t cursor_y = y;

    while (*str) {
        if (*str == '\n') {
            cursor_y += 8 * size + 2; 
            cursor_x = x;
        } else {
            display_draw_char(cursor_x, cursor_y, *str, color, bg, size);
            cursor_x += 6 * size;    
        }
        str++;
    }
}