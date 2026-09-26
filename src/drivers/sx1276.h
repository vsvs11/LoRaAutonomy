#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef struct {
    int freq;
    int bw_idx;
    int cr_idx;
    int sf;
    bool crc_on;
} lora_config_t;


void lora_reg_write(int reg, int data);

int lora_reg_read(int reg);

void lora_idle(void);

void lora_sleep(void);

void lora_receive(void);

void lora_set_preamble_length(long length);

int lora_init(lora_config_t *config);

void lora_send_packet(uint8_t *buf, int size);

int lora_receive_packet(uint8_t *buf, int size);

int lora_packet_rssi(void);

float lora_packet_snr(void);

void lora_close(void);