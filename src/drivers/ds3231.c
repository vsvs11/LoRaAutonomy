#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "driver/i2c.h"
#include "soc/gpio_struct.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "config.h"
#include <drivers/ds3231.h>


/*
 * Register definitions
 */
#define REG_SECONDS                    0x00
#define REG_MINUTES                    0x01
#define REG_HOURS                      0x02
#define REG_DAY                        0x03
#define REG_DATE                       0x04
#define REG_MONTH                      0x05
#define REG_YEAR                       0x06
#define REG_ALARM1_SEC                 0x07
#define REG_ALARM1_MIN                 0x08
#define REG_ALARM1_HOUR                0x09
#define REG_ALARM1_DAY_DATE            0x0A
#define REG_ALARM2_MIN                 0x0B
#define REG_ALARM2_HOUR                0x0C
#define REG_ALARM2_DAY_DATE            0x0D
#define REG_CONTROL                    0x0E
#define REG_STATUS                     0x0F
#define REG_AGING_OFFSET               0x10
#define REG_TEMP_MSB                   0x11
#define REG_TEMP_LSB                   0x12

/*
 * Control register bits (0x0E)
 */
#define CTRL_EOSC                      (1 << 7)
#define CTRL_BBSQW                     (1 << 6)
#define CTRL_CONV                      (1 << 5)
#define CTRL_RS2                       (1 << 4)
#define CTRL_RS1                       (1 << 3)
#define CTRL_INTCN                     (1 << 2)
#define CTRL_A2IE                      (1 << 1)
#define CTRL_A1IE                      (1 << 0)

/*
 * Status register bits (0x0F)
 */
#define STAT_OSF                       (1 << 7)
#define STAT_EN32KHZ                   (1 << 3)
#define STAT_BSY                       (1 << 2)
#define STAT_A2F                       (1 << 1)
#define STAT_A1F                       (1 << 0)

#define I2C_MASTER_PORT                I2C_NUM_0
#define DS3231_ADDR                    0x68


static uint8_t dec_to_bcd(uint8_t val){
    return (uint8_t)(((val / 10) << 4) | (val % 10));
}

static uint8_t bcd_to_dec(uint8_t val){
    return (uint8_t)(((val >> 4) * 10) + (val & 0x0F));
}

static int ds3231_write_bytes(uint8_t reg, const uint8_t *data, size_t len){
    uint8_t buff[len + 1];
    buff[0] = reg;
    memcpy(&buff[1], data, len);

    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_PORT, DS3231_ADDR, buff, len + 1, pdMS_TO_TICKS(100));
    return (err == ESP_OK) ? 1 : -1;
}

static int ds3231_read_bytes(uint8_t reg, uint8_t *data, size_t len){
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_PORT, DS3231_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
    return (err == ESP_OK) ? 1 : -1;
}

int ds3231_reg_write(uint8_t reg, uint8_t data){
    /*
     * Write 8-bit register
     */
    uint8_t buff[2] = {reg, data};
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_PORT, DS3231_ADDR, buff, sizeof(buff), pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
        return 1;
    } else {
        return -1;
    }
}

int ds3231_reg_read(uint8_t reg){
    /*
     * Read 8-bit register
     */
    uint8_t data = 0;
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_PORT, DS3231_ADDR, &reg, 1, &data, 1, pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
        return data;
    } else {
        return -1;
    }
}

int ds3231_init(ds3231_config_t *config){
    /*
     * Initialization I2C device and DS3231 chip
     */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = DS_SDA_NUM,
        .scl_io_num = DS_SCL_NUM,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = DS_I2C_FREQ,
    };
    i2c_param_config(I2C_MASTER_PORT, &conf);
    i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);

    int status = ds3231_reg_read(REG_STATUS);
    if (status == -1) {
        return -1;
    }

    int ctrl = ds3231_reg_read(REG_CONTROL);
    if (ctrl == -1) {
        return -1;
    }

    /*  initialization and control/status register adjustment  */
    ctrl &= ~CTRL_EOSC; // Включаем осциллятор

    if (config != NULL) {
        ctrl &= ~(CTRL_RS1 | CTRL_RS2);
        ctrl |= ((config->sqw_rate & 0x03) << 3);

        if (config->enable_sqw) {
            ctrl &= ~CTRL_INTCN;
        } else {
            ctrl |= CTRL_INTCN;
        }

        if (config->enable_32khz) {
            status |= STAT_EN32KHZ;
        } else {
            status &= ~STAT_EN32KHZ;
        }
    } else {
        ctrl |= CTRL_INTCN;
        status &= ~STAT_EN32KHZ;
    }

    ds3231_reg_write(REG_CONTROL, (uint8_t)ctrl);
    ds3231_reg_write(REG_STATUS, (uint8_t)status);

    return 1;
}

int ds3231_set_time(const ds3231_time_t *time){
    if (time == NULL) {
        return -1;
    }

    uint8_t raw[7];
    raw[0] = dec_to_bcd(time->seconds % 60);
    raw[1] = dec_to_bcd(time->minutes % 60);
    raw[2] = dec_to_bcd(time->hours % 24); // 24-hour mode (bit 6 = 0)
    raw[3] = dec_to_bcd((time->day >= 1 && time->day <= 7) ? time->day : 1);
    raw[4] = dec_to_bcd((time->date >= 1 && time->date <= 31) ? time->date : 1);

    uint8_t century = (time->year >= 2000) ? 0x80 : 0x00;
    uint8_t yr = (time->year >= 2000) ? (uint8_t)(time->year - 2000) : (uint8_t)(time->year % 100);

    raw[5] = dec_to_bcd((time->month >= 1 && time->month <= 12) ? time->month : 1) | century;
    raw[6] = dec_to_bcd(yr);

    if (ds3231_write_bytes(REG_SECONDS, raw, sizeof(raw)) != 1) {
        return -1;
    }

    // Сбрасываем флаг остановки осциллятора (OSF) после установки времени
    int status = ds3231_reg_read(REG_STATUS);
    if (status != -1) {
        ds3231_reg_write(REG_STATUS, (uint8_t)(status & ~STAT_OSF));
    }

    return 1;
}

int ds3231_get_time(ds3231_time_t *time){
    if (time == NULL) {
        return -1;
    }

    uint8_t raw[7] = {0};
    if (ds3231_read_bytes(REG_SECONDS, raw, sizeof(raw)) != 1) {
        return -1;
    }

    time->seconds = bcd_to_dec(raw[0] & 0x7F);
    time->minutes = bcd_to_dec(raw[1] & 0x7F);

    if (raw[2] & 0x40) {
        // 12-hour mode
        uint8_t hr = bcd_to_dec(raw[2] & 0x1F);
        bool pm = (raw[2] & 0x20) != 0;
        if (hr == 12) hr = 0;
        time->hours = pm ? (hr + 12) : hr;
    } else {
        // 24-hour mode
        time->hours = bcd_to_dec(raw[2] & 0x3F);
    }

    time->day   = bcd_to_dec(raw[3] & 0x07);
    time->date  = bcd_to_dec(raw[4] & 0x3F);
    time->month = bcd_to_dec(raw[5] & 0x1F);
    time->year  = 2000 + bcd_to_dec(raw[6]);

    return 1;
}

/**
 * Return chip temperature in Celsius (resolution 0.25 C).
 */
float ds3231_temperature(void){
    uint8_t raw[2] = {0};
    if (ds3231_read_bytes(REG_TEMP_MSB, raw, 2) != 1) {
        return 0.0f;
    }

    int8_t msb = (int8_t)raw[0];
    uint8_t frac = (raw[1] >> 6) & 0x03;

    return (float)msb + ((float)frac * 0.25f);
}

/**
 * Return true if oscillator stop flag (OSF) is set.
 */
bool ds3231_lost_power(void){
    int status = ds3231_reg_read(REG_STATUS);
    if (status == -1) {
        return true;
    }
    return (status & STAT_OSF) != 0;
}

void ds3231_clear_alarm_flags(void){
    int status = ds3231_reg_read(REG_STATUS);
    if (status != -1) {
        ds3231_reg_write(REG_STATUS, (uint8_t)(status & ~(STAT_A1F | STAT_A2F)));
    }
}

void ds3231_sleep(void){
    int status = ds3231_reg_read(REG_STATUS);
    if (status != -1) {
        ds3231_reg_write(REG_STATUS, (uint8_t)(status & ~STAT_EN32KHZ));
    }

    int ctrl = ds3231_reg_read(REG_CONTROL);
    if (ctrl != -1) {
        ds3231_reg_write(REG_CONTROL, (uint8_t)((ctrl | CTRL_INTCN) & ~CTRL_BBSQW));
    }
}

void ds3231_wakeup(void){
    int ctrl = ds3231_reg_read(REG_CONTROL);
    if (ctrl != -1) {
        ds3231_reg_write(REG_CONTROL, (uint8_t)(ctrl & ~CTRL_EOSC));
    }
}

/**
 * Shutdown hardware.
 */
void ds3231_close(void){
    ds3231_sleep();
    i2c_driver_delete(I2C_MASTER_PORT);
}
