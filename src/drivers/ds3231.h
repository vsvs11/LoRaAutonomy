#ifndef DS3231_H
#define DS3231_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint8_t seconds;    // 0..59
    uint8_t minutes;    // 0..59
    uint8_t hours;      // 0..23
    uint8_t day;        // 1..7 (day of week)
    uint8_t date;       // 1..31
    uint8_t month;      // 1..12
    uint16_t year;      // 2000..2099
} ds3231_time_t;

typedef struct {
    bool enable_32khz;
    bool enable_sqw;
    uint8_t sqw_rate;   // 0: 1Hz, 1: 1.024kHz, 2: 4.096kHz, 3: 8.192kHz
} ds3231_config_t;

int ds3231_reg_write(uint8_t reg, uint8_t data);

int ds3231_reg_read(uint8_t reg);

int ds3231_init(ds3231_config_t *config);

int ds3231_set_time(const ds3231_time_t *time);

int ds3231_get_time(ds3231_time_t *time);

float ds3231_temperature(void);

bool ds3231_lost_power(void);

void ds3231_clear_alarm_flags(void);

void ds3231_sleep(void);

void ds3231_wakeup(void);

void ds3231_close(void);

#endif // DS3231_H
