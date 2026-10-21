#ifndef BQ27441_G1_H
#define BQ27441_G1_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint16_t design_capacity_mah;
    uint16_t design_energy_mwh;
    uint16_t terminate_voltage_mv;
    uint16_t taper_rate;
} bq_config_t;

typedef struct {
    uint8_t state_block[32];
    bool valid;
} bq_snapshot_t;

int bq_reg_write(uint8_t reg, uint16_t *data, size_t size);

int bq_reg_read(uint8_t reg);

int bq_init(bq_config_t *config);

void bq_snapshot_create_default(bq_config_t *config, bq_snapshot_t *snap);

int bq_snapshot_dump(bq_snapshot_t *snap);

int bq_snapshot_load(const bq_snapshot_t *snap);

int bq_voltage(void);

int16_t bq_current(void);

int bq_soc(void);

int bq_remaining_capacity(void);

int bq_full_capacity(void);

int16_t bq_power(void);

float bq_temperature(void);

int bq_soh(void);

void bq_reset(void);

void bq_sleep(void);

void bq_wakeup(void);

void bq_close(void);

#endif // BQ27441_G1_H
