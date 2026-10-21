#include "atgm336h.h"
#include "esp_err.h"

int atgm_init(void){
    uart_config_t uart_config = {
        .baud_rate = ATGM336H_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t err = uart_driver_install(ATGM336H_UART_PORT, ATGM336H_RX_BUF_SIZE, 0, 0, NULL, 0);
    if (err != ESP_OK){
        return -1;
    }

    err = uart_param_config(ATGM336H_UART_PORT, &uart_config);
    if (err != ESP_OK){
        return -1;
    }

    err = uart_set_pin(ATGM336H_UART_PORT, ATGM336H_TX_PIN, ATGM336H_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK){
        return -1;
    }

    return 0;
}

int atgm_write(const uint8_t *data, size_t size){
    int written = uart_write_bytes(ATGM336H_UART_PORT, (const char *)data, size);
    if (written < 0){
        return -1;
    }
    return written;
}

int atgm_read(uint8_t *data, size_t max_size, uint32_t timeout_ms){
    int len = uart_read_bytes(ATGM336H_UART_PORT, data, max_size, pdMS_TO_TICKS(timeout_ms));
    if (len < 0){
        return -1;
    }
    return len;
}
