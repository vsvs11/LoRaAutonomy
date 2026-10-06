#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "driver/i2c.h"
#include "soc/gpio_struct.h"

/*  Standard register   */

// Read Of Write Register in 2-byte chunks

#define REG_CONTROL                              0x00
#define REG_TEMPERATURE                          0x02
#define REG_VOLTAGE                              0x04
#define REG_FLAGS                                0x06
#define REG_NOMINAL_AVAILABLE_CAPACITY           0x08
#define REG_FULL_AVAILABLE_CAPACITY              0x0A
#define REG_REMAINING_CAPACITY                   0x0C
#define REG_FULL_CHARGE_CAPACITY                 0x0E
#define REG_AVERAGE_CURRENT                      0x10
#define REG_STANDBY_CURRENT                      0x12
#define REG_MAX_LOAD_CURRENT                     0x14
#define REG_AVERAGE_POWER                        0x18
#define REG_STATE_OF_CHARGE                      0x1C
#define REG_INTERNAL_TEMPERATURE                 0x1E
#define REG_STATE_OF_HEALTH                      0x20
#define REG_REMAINING_CAPACITY_UNFILTERED        0x28
#define REG_REMAINING_CAPACITY_FILTERED          0x2A
#define REG_FULL_CHARGE_CAPACITY_UNFILTERED      0x2C
#define REG_FULL_CHARGE_CAPACITY_FILTERED        0x2E
#define REG_STATE_OF_CHARGE_UNFILTERED           0x30
#define REG_TRUE_REMAINING_CAPACITY              0x6A




#define I2C_MASTER_PORT                         I2C_NUM_0
#define BQ27441_ADDR                            0x55 // Базовый 7-битный адрес чипа

/*  Pins Configuration  */

#define SCL_NUM                                 22
#define SDA_NUM                                 21
#define I2C_FREQ                                100000

int bq_reg_write(uint8_t reg, uint16_t *data, size_t size){
    int size_buff =  1 + size * 2;
    uint8_t buff[size_buff];

    buff[0] = reg;
    int a = 0;
    for (int i = 1; i < size_buff; i+=2){
        buff[1 + (a * 2)]     = (uint8_t)(data[a] & 0xFF);
        buff[1 + (a * 2) + 1] = (uint8_t)((data[a] >> 8) & 0xFF);
        a++;
    }

    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_PORT, BQ27441_ADDR, buff, size_buff, pdMS_TO_TICKS(100));
      if (err == ESP_OK){
        return 1;
    }else{
        return -1;
    }
}

int bq_reg_read(uint8_t reg){
    uint8_t data[2] = {0};
    esp_err_t err =  i2c_master_write_read_device(I2C_MASTER_PORT, BQ27441_ADDR, &reg, 1, data, 2, pdMS_TO_TICKS(100));
    if (err == ESP_OK){
        return (data[1] << 8) | data[0];
    }else{
        return -1;
    }

}
void bq_init(){
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_NUM,
        .scl_io_num = SCL_NUM,
        .sda_pullup_en = GPIO_PULLUP_ENABLE, // Включаем внутренние подтяжки ESP32
        .scl_pullup_en = GPIO_PULLUP_ENABLE, // (но лучше иметь внешние резисторы на плате)
        .master.clk_speed = I2C_FREQ,
    };
    i2c_param_config(I2C_MASTER_PORT, &conf);
    i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);
    int flags = bq_reg_read(REG_FLAGS);

    if (flags == -1){
        return -1;
    }
    /*  Написать функции для создания дефолтного снапшота (на основе макросов и наружнего конфига),
        выгрузки снапшота из RAM чипа питания и загрузки снапшота в RAM чипа питания    */
    
    
}
