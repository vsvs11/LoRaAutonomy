#pragma once

void display_init(void);

void display_cmd(uint8_t cmd);

void display_data_byte(uint8_t data);

void display_data(void *data, size_t len);

void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

void display_fill_screen(uint16_t color);