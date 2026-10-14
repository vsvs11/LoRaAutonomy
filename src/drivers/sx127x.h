#pragma once

#include <stddef.h>
#include <stdbool.h>

#define DIO0_RX_DONE                   0x00  // DIO0 -> RxDone
#define DIO0_TX_DONE                   0x40  // DIO0 -> TxDone

#define MODE_RX_CONTINUOUS             0x05
#define IRQ_TX_DONE_MASK               0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK     0x20
#define IRQ_RX_DONE_MASK               0x40

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

void lora_start_rx_continuous(void);

void lora_send_packet_async(uint8_t *buf, int size);