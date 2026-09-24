#pragma once


void display_init(void);

void display_cmd(uint8_t cmd);

void display_data_byte(uint8_t data);

void display_data(void *data, size_t len);

void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

void display_fill_screen(uint16_t color);

void display_fill_rect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);

void display_fill_circle(uint16_t x0, uint16_t y0, uint16_t r, uint16_t color);

void display_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color);

void display_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg, uint8_t size);

void display_draw_string(int16_t x, int16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t size);