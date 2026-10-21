#pragma once

            /*  kernel  */
#define SUBSYS_COUNT                   4
#define HEARTBEAT_TIMEOUT_MS           5000

            /*  list  */
#define TASKS_MAX                      15

        /*  drivers/ili9341  */
#define DISP_MOSI_NUM                  11
#define DISP_SCLK_NUM                  12
#define DISP_RESET_NUM                 5
#define DISP_DC_NUM                    4
#define DISP_CS_NUM                    18
#define DISP_MAX_TRANSFER_SZ           15000
#define DISP_FREQ                      60000000

        /*  drivers/sx127x  */
#define SX_MOSI_NUM                    5
#define SX_MISO_NUM                    4
#define SX_SCLK_NUM                    6
#define SX_RESET_NUM                   1
#define SX_CS_NUM                      7
#define SX_FREQ                        4000000

       /*  drivers/dq27441_g1  */
#define BQ_SCL_NUM                     22
#define BQ_SDA_NUM                     21
#define BQ_I2C_FREQ                    100000

        /*  drivers/ds3231  */
#define DS_SCL_NUM                     22
#define DS_SDA_NUM                     21
#define DS_I2C_FREQ                    100000

        /*  drivers/atgm336h  */
#define ATGM_UART_PORT                 1
#define ATGM_TX_NUM                    17
#define ATGM_RX_NUM                    16
#define ATGM_RESET_NUM                 -1
#define ATGM_ONOFF_NUM                 -1
#define ATGM_RX_BUF_SIZE               1024
