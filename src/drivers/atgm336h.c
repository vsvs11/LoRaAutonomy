#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include <drivers/atgm336h.h>


/*
 * CASIC (PCAS) Command definitions
 */
#define PCAS_CMD_HOT_START             "$PCAS10,0*1C\r\n"
#define PCAS_CMD_WARM_START            "$PCAS10,1*1D\r\n"
#define PCAS_CMD_COLD_START            "$PCAS10,2*1E\r\n"

#define PCAS_CMD_RATE_1HZ              "$PCAS02,1000*2E\r\n"
#define PCAS_CMD_RATE_2HZ              "$PCAS02,500*1A\r\n"
#define PCAS_CMD_RATE_5HZ              "$PCAS02,200*1D\r\n"
#define PCAS_CMD_RATE_10HZ             "$PCAS02,100*1E\r\n"

#define PCAS_CMD_MODE_GPS              "$PCAS04,1*18\r\n"
#define PCAS_CMD_MODE_BDS              "$PCAS04,2*1B\r\n"
#define PCAS_CMD_MODE_GPS_BDS          "$PCAS04,3*1A\r\n"
#define PCAS_CMD_MODE_GLONASS          "$PCAS04,4*1D\r\n"
#define PCAS_CMD_MODE_GPS_GLONASS      "$PCAS04,5*1C\r\n"
#define PCAS_CMD_MODE_ALL              "$PCAS04,7*1E\r\n"

/*
 * Reset types
 */
#define RESET_HOT                      0
#define RESET_WARM                     1
#define RESET_COLD                     2


static int __baud_rate = 9600;

static uint8_t atgm_calc_checksum(const char *cmd){
    uint8_t crc = 0;
    if (*cmd == '$') cmd++;
    while (*cmd && *cmd != '*') {
        crc ^= (uint8_t)(*cmd++);
    }
    return crc;
}

void atgm_send_cmd(const char *cmd){
    /*
     * Write command to UART
     */
    uart_write_bytes(ATGM_UART_PORT, cmd, strlen(cmd));
}

void atgm_set_mode(int nav_mode){
    switch (nav_mode) {
        case 1: atgm_send_cmd(PCAS_CMD_MODE_GPS); break;
        case 2: atgm_send_cmd(PCAS_CMD_MODE_BDS); break;
        case 3: atgm_send_cmd(PCAS_CMD_MODE_GPS_BDS); break;
        case 4: atgm_send_cmd(PCAS_CMD_MODE_GLONASS); break;
        case 5: atgm_send_cmd(PCAS_CMD_MODE_GPS_GLONASS); break;
        default: atgm_send_cmd(PCAS_CMD_MODE_ALL); break;
    }
}

void atgm_set_update_rate(int rate_hz){
    switch (rate_hz) {
        case 10: atgm_send_cmd(PCAS_CMD_RATE_10HZ); break;
        case 5:  atgm_send_cmd(PCAS_CMD_RATE_5HZ); break;
        case 2:  atgm_send_cmd(PCAS_CMD_RATE_2HZ); break;
        default: atgm_send_cmd(PCAS_CMD_RATE_1HZ); break;
    }
}

void atgm_set_baudrate(int baud){
    int code = 1; // 9600 default
    switch (baud) {
        case 4800:   code = 0; break;
        case 9600:   code = 1; break;
        case 19200:  code = 2; break;
        case 38400:  code = 3; break;
        case 57600:  code = 4; break;
        case 115200: code = 5; break;
        default:     code = 1; baud = 9600; break;
    }

    char body[24];
    snprintf(body, sizeof(body), "PCAS01,%d", code);
    uint8_t crc = atgm_calc_checksum(body);

    char cmd[32];
    snprintf(cmd, sizeof(cmd), "$%s*%02X\r\n", body, crc);
    atgm_send_cmd(cmd);

    uart_wait_tx_done(ATGM_UART_PORT, pdMS_TO_TICKS(100));
    uart_set_baudrate(ATGM_UART_PORT, baud);
    __baud_rate = baud;
}

void atgm_reset(int reset_type){
    if (reset_type == RESET_COLD) {
        atgm_send_cmd(PCAS_CMD_COLD_START);
    } else if (reset_type == RESET_WARM) {
        atgm_send_cmd(PCAS_CMD_WARM_START);
    } else {
        atgm_send_cmd(PCAS_CMD_HOT_START);
    }
}

void atgm_sleep(void){
#if (ATGM_ONOFF_NUM >= 0)
    gpio_set_level(ATGM_ONOFF_NUM, 0);
#else
    uart_flush_input(ATGM_UART_PORT);
#endif
}

void atgm_wakeup(void){
#if (ATGM_ONOFF_NUM >= 0)
    gpio_set_level(ATGM_ONOFF_NUM, 1);
#else
    atgm_send_cmd("\r\n");
#endif
}

int atgm_init(atgm_config_t *config){
    /*
     * Initialization UART and ATGM336H chip
     */
#if (ATGM_RESET_NUM >= 0)
    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL << ATGM_RESET_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&rst_conf);
    gpio_set_level(ATGM_RESET_NUM, 0);
    esp_rom_delay_us(10000);
    gpio_set_level(ATGM_RESET_NUM, 1);
    esp_rom_delay_us(50000);
#endif

#if (ATGM_ONOFF_NUM >= 0)
    gpio_config_t pwr_conf = {
        .pin_bit_mask = (1ULL << ATGM_ONOFF_NUM),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&pwr_conf);
    gpio_set_level(ATGM_ONOFF_NUM, 1);
#endif

    __baud_rate = (config && config->baud_rate > 0) ? config->baud_rate : 9600;

    uart_config_t uart_conf = {
        .baud_rate = 9600, // ATGM336H стартует на 9600 по умолчанию
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    uart_driver_install(ATGM_UART_PORT, ATGM_RX_BUF_SIZE, 0, 0, NULL, 0);
    uart_param_config(ATGM_UART_PORT, &uart_conf);
    uart_set_pin(ATGM_UART_PORT, ATGM_TX_NUM, ATGM_RX_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    /* initialization and mode adjustment */
    if (config != NULL) {
        if (config->baud_rate > 0 && config->baud_rate != 9600) {
            atgm_set_baudrate(config->baud_rate);
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (config->nav_mode > 0) {
            atgm_set_mode(config->nav_mode);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        if (config->update_rate_hz > 0) {
            atgm_set_update_rate(config->update_rate_hz);
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }

    uart_flush_input(ATGM_UART_PORT);
    return 1;
}

int atgm_receive_packet(uint8_t *buf, int size){
    size_t buffered_len = 0;
    uart_get_buffered_data_len(ATGM_UART_PORT, &buffered_len);

    // Если данных в буфере нет — сразу выходим, ничего не трогая
    if (buffered_len == 0) {
        return 0;
    }

    if (buffered_len > (size_t)size) {
        buffered_len = size;
    }

    int len = uart_read_bytes(ATGM_UART_PORT, buf, buffered_len, 0);
    return len;
}

int atgm_receive_line(char *buf, int max_size){
    size_t buffered_len = 0;
    uart_get_buffered_data_len(ATGM_UART_PORT, &buffered_len);
    if (buffered_len == 0 || max_size <= 1) {
        return 0;
    }

    int idx = 0;
    uint8_t ch = 0;
    bool started = false;

    while (idx < (max_size - 1)) {
        int r = uart_read_bytes(ATGM_UART_PORT, &ch, 1, pdMS_TO_TICKS(5));
        if (r <= 0) {
            break;
        }
        if (!started) {
            if (ch == '$') {
                started = true;
                buf[idx++] = (char)ch;
            }
            continue;
        }
        buf[idx++] = (char)ch;
        if (ch == '\n') {
            break;
        }
    }

    buf[idx] = '\0';
    return idx;
}

/**
 * Shutdown hardware.
 */
void atgm_close(void){
    atgm_sleep();
    uart_driver_delete(ATGM_UART_PORT);
}
