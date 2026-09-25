#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "driver/spi_master.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>


/*
 * Register definitions
 */
#define REG_FIFO                       0x00
#define REG_OP_MODE                    0x01
#define REG_FRF_MSB                    0x06
#define REG_FRF_MID                    0x07
#define REG_FRF_LSB                    0x08
#define REG_PA_CONFIG                  0x09
#define REG_LNA                        0x0c
#define REG_FIFO_ADDR_PTR              0x0d
#define REG_FIFO_TX_BASE_ADDR          0x0e
#define REG_FIFO_RX_BASE_ADDR          0x0f
#define REG_FIFO_RX_CURRENT_ADDR       0x10
#define REG_IRQ_FLAGS                  0x12
#define REG_RX_NB_BYTES                0x13
#define REG_PKT_SNR_VALUE              0x19
#define REG_PKT_RSSI_VALUE             0x1a
#define REG_MODEM_CONFIG_1             0x1d
#define REG_MODEM_CONFIG_2             0x1e
#define REG_PREAMBLE_MSB               0x20
#define REG_PREAMBLE_LSB               0x21
#define REG_PAYLOAD_LENGTH             0x22
#define REG_MODEM_CONFIG_3             0x26
#define REG_RSSI_WIDEBAND              0x2c
#define REG_DETECTION_OPTIMIZE         0x31
#define REG_DETECTION_THRESHOLD        0x37
#define REG_SYNC_WORD                  0x39
#define REG_DIO_MAPPING_1              0x40
#define REG_VERSION                    0x42

/*
 * Transceiver modes
 */
#define MODE_LONG_RANGE_MODE           0x80
#define MODE_SLEEP                     0x00
#define MODE_STDBY                     0x01
#define MODE_TX                        0x03
#define MODE_RX_CONTINUOUS             0x05
#define MODE_RX_SINGLE                 0x06

/*
 * PA configuration
 */
#define PA_BOOST                       0x80

/*
 * IRQ masks
 */
#define IRQ_TX_DONE_MASK               0x08
#define IRQ_PAYLOAD_CRC_ERROR_MASK     0x20
#define IRQ_RX_DONE_MASK               0x40

#define PA_OUTPUT_RFO_PIN              0
#define PA_OUTPUT_PA_BOOST_PIN         1

#define TIMEOUT_RESET                  100


#define MOSI_NUM                       11
#define MISO_NUM                       13
#define SCLK_NUM                       12
#define RESET_NUM                      5
#define CS_NUM                         14

static spi_device_handle_t spi_dev;

void lora_reg_write(int reg, int data){
    uint8_t out[2] = {0x80 | reg, data};
    uint8_t in[2];

    spi_transaction_t t = {
        .flags = 0,
        .length = 8 * sizeof(out),
        .tx_buffer = out,
        .rx_buffer = in
    };
}

int lora_reg_read(int reg){
    uint8_t out[2] = {reg, 0xFF};
    uint8_t in[2];

    spi_transaction_t t = {
        .flags = 0,
        .length = 8 * sizeof(out),
        .tx_buffer = out,
        .rx_buffer = in
    };
}

void lora_init(void){
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << RESET_NUM),
        .mode = GPIO_MODE_OUTPUT,            
        .pull_up_en = GPIO_PULLUP_DISABLE,     
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE       
    };
    gpio_config(&gpio_conf);
    gpio_set_level(RESET_NUM,0);
    esp_rom_delay_us(200);
    gpio_set_level(RESET_NUM,1);
    esp_rom_delay_us(10);

    spi_bus_config_t buscfg = {
        .mosi_io_num = MOSI_NUM,
        .miso_io_num = MISO_NUM,
        .sclk_io_num = SCLK_NUM,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 257
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 9000000,
        .mode = 0,
        .spics_io_num = CS_NUM,
        .queue_size = 7
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &spi_dev);
    lora_reg_write(0x01, 0x80); //включаем лору и переводим спящий режим
    lora_reg_write(0x06, 0x6C);
    lora_reg_write(0x07, 0x80);
    lora_reg_write(0x08, 0x00); //задаем F step
    lora_reg_write(0x09, 0x8F); //выходная мощность
    lora_reg_write(0x1D, 0x72); //BW и CR
    lora_reg_write(0x1E, 0x74); //SF
    lora_reg_write(0x26, 0x04); //AGS
    lora_reg_write(0x39, 0x12); //Sync Word
    lora_reg_write(0x0E, 0x00);
    lora_reg_write(0x0F, 0x00);//FIFO
    lora_reg_write(0x01, 0x81);//Standby 
    
}