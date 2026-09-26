#pragma once

#include <stdbool.h>
#include <stdint.h>


bool ds18b20_reset(void);

void ds18b20_write_bit(uint8_t bit);

uint8_t ds18b20_read_bit(void);

void ds18b20_write_byte(uint8_t data);

uint8_t ds18b20_read_byte(void);

float ds18b20_read_temp(void);

int ds18b20_init(void);