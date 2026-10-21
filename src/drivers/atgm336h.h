#ifndef ATGM336H_H
#define ATGM336H_H

#include <stdint.h>
#include <stddef.h>
#include "driver/uart.h"
#include "driver/gpio.h"

#define ATGM336H_UART_PORT      UART_NUM_1
#define ATGM336H_TX_PIN         GPIO_NUM_17
#define ATGM336H_RX_PIN         GPIO_NUM_16
#define ATGM336H_BAUD_RATE      9600
#define ATGM336H_RX_BUF_SIZE    1024

// Стандартные команды управления CASIC (PCAS) для ATGM336H
#define ATGM_CMD_HOT_START      "$PCAS10,0*1C\r\n"
#define ATGM_CMD_WARM_START     "$PCAS10,1*1D\r\n"
#define ATGM_CMD_COLD_START     "$PCAS10,2*1E\r\n"

#define ATGM_CMD_MODE_GPS_BDS   "$PCAS04,3*1A\r\n"
#define ATGM_CMD_MODE_ALL       "$PCAS04,7*1E\r\n"

int atgm_init(void);
int atgm_write(const uint8_t *data, size_t size);
int atgm_read(uint8_t *data, size_t max_size, uint32_t timeout_ms);

#endif // ATGM336H_H
