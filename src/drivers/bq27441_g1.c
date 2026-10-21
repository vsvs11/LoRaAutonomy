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
#include <drivers/bq27441_g1.h>


/*
 * Standard register definitions (2-byte words)
 */
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

/*
 * Extended data commands
 */
#define REG_DATA_CLASS                           0x3E
#define REG_DATA_BLOCK                           0x3F
#define REG_BLOCK_DATA                           0x40
#define REG_BLOCK_DATA_CHECKSUM                  0x60
#define REG_BLOCK_DATA_CONTROL                   0x61

/*
 * Control subcommands
 */
#define CTRL_CONTROL_STATUS                      0x0000
#define CTRL_DEVICE_TYPE                         0x0001
#define CTRL_SET_HIBERNATE                       0x0011
#define CTRL_CLEAR_HIBERNATE                     0x0012
#define CTRL_SET_CFGUPDATE                       0x0013
#define CTRL_SEALED                              0x0020
#define CTRL_SOFT_RESET                          0x0042
#define CTRL_EXIT_CFGUPDATE                      0x0043
#define CTRL_UNSEAL_KEY                          0x8000

/*
 * Flags masks & Subclasses
 */
#define FLAG_ITPOR                               (1 << 5)
#define FLAG_CFGUPMODE                           (1 << 4)
#define SUBCLASS_STATE                           0x52

/*
 * Offsets inside Subclass 0x52 (State) block 0
 */
#define OFFSET_DESIGN_CAPACITY                   10
#define OFFSET_DESIGN_ENERGY                     12
#define OFFSET_TERMINATE_VOLTAGE                 16
#define OFFSET_TAPER_RATE                        27

/*
 * Default battery parameters
 */
#define DEFAULT_DESIGN_CAP_MAH                   2000
#define DEFAULT_DESIGN_ENERGY_MWH                7400
#define DEFAULT_TERM_VOLTAGE_MV                  3200
#define DEFAULT_TAPER_RATE                       100

#define I2C_MASTER_PORT                          I2C_NUM_0
#define BQ27441_ADDR                             0x55


static int bq_write_bytes(uint8_t reg, const uint8_t *data, size_t len){
    uint8_t buff[len + 1];
    buff[0] = reg;
    memcpy(&buff[1], data, len);

    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_PORT, BQ27441_ADDR, buff, len + 1, pdMS_TO_TICKS(100));
    return (err == ESP_OK) ? 1 : -1;
}

static int bq_read_bytes(uint8_t reg, uint8_t *data, size_t len){
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_PORT, BQ27441_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
    return (err == ESP_OK) ? 1 : -1;
}

int bq_reg_write(uint8_t reg, uint16_t *data, size_t size){
    /*
     * Write register in 2-byte chunks
     */
    int size_buff = 1 + size * 2;
    uint8_t buff[size_buff];

    buff[0] = reg;
    int a = 0;
    for (int i = 1; i < size_buff; i += 2) {
        buff[1 + (a * 2)]     = (uint8_t)(data[a] & 0xFF);
        buff[1 + (a * 2) + 1] = (uint8_t)((data[a] >> 8) & 0xFF);
        a++;
    }

    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_PORT, BQ27441_ADDR, buff, size_buff, pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
        return 1;
    } else {
        return -1;
    }
}

int bq_reg_read(uint8_t reg){
    /*
     * Read 16-bit word register
     */
    uint8_t data[2] = {0};
    esp_err_t err = i2c_master_write_read_device(I2C_MASTER_PORT, BQ27441_ADDR, &reg, 1, data, 2, pdMS_TO_TICKS(100));
    if (err == ESP_OK) {
        return (data[1] << 8) | data[0];
    } else {
        return -1;
    }
}

static void bq_ctrl_write(uint16_t subcmd){
    bq_reg_write(REG_CONTROL, &subcmd, 1);
}

static void bq_unseal(void){
    bq_ctrl_write(CTRL_UNSEAL_KEY);
    bq_ctrl_write(CTRL_UNSEAL_KEY);
    vTaskDelay(pdMS_TO_TICKS(5));
}

static void bq_seal(void){
    bq_ctrl_write(CTRL_SEALED);
    vTaskDelay(pdMS_TO_TICKS(5));
}

static int bq_enter_cfgupdate(void){
    bq_unseal();
    bq_ctrl_write(CTRL_SET_CFGUPDATE);

    int timeout = 100;
    while (timeout-- > 0) {
        int flags = bq_reg_read(REG_FLAGS);
        if (flags != -1 && (flags & FLAG_CFGUPMODE)) {
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return -1;
}

static int bq_exit_cfgupdate(void){
    bq_ctrl_write(CTRL_SOFT_RESET);

    int timeout = 100;
    while (timeout-- > 0) {
        int flags = bq_reg_read(REG_FLAGS);
        if (flags != -1 && ((flags & FLAG_CFGUPMODE) == 0)) {
            bq_seal();
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    return -1;
}

static uint8_t bq_calc_checksum(const uint8_t *block){
    uint8_t sum = 0;
    for (int i = 0; i < 32; i++) {
        sum += block[i];
    }
    return (uint8_t)(255 - sum);
}

void bq_snapshot_create_default(bq_config_t *config, bq_snapshot_t *snap){
    if (snap == NULL) return;

    if (bq_snapshot_dump(snap) != 1) {
        memset(snap->state_block, 0, sizeof(snap->state_block));
    }

    uint16_t cap   = (config && config->design_capacity_mah > 0)  ? config->design_capacity_mah  : DEFAULT_DESIGN_CAP_MAH;
    uint16_t nrg   = (config && config->design_energy_mwh > 0)    ? config->design_energy_mwh    : DEFAULT_DESIGN_ENERGY_MWH;
    uint16_t termv = (config && config->terminate_voltage_mv > 0) ? config->terminate_voltage_mv : DEFAULT_TERM_VOLTAGE_MV;
    uint16_t taper = (config && config->taper_rate > 0)           ? config->taper_rate           : DEFAULT_TAPER_RATE;

    // В Extended Data параметры хранятся в Big-Endian (MSB, затем LSB)
    snap->state_block[OFFSET_DESIGN_CAPACITY]     = (uint8_t)(cap >> 8);
    snap->state_block[OFFSET_DESIGN_CAPACITY + 1] = (uint8_t)(cap & 0xFF);

    snap->state_block[OFFSET_DESIGN_ENERGY]       = (uint8_t)(nrg >> 8);
    snap->state_block[OFFSET_DESIGN_ENERGY + 1]   = (uint8_t)(nrg & 0xFF);

    snap->state_block[OFFSET_TERMINATE_VOLTAGE]     = (uint8_t)(termv >> 8);
    snap->state_block[OFFSET_TERMINATE_VOLTAGE + 1] = (uint8_t)(termv & 0xFF);

    snap->state_block[OFFSET_TAPER_RATE]          = (uint8_t)(taper >> 8);
    snap->state_block[OFFSET_TAPER_RATE + 1]      = (uint8_t)(taper & 0xFF);

    snap->valid = true;
}

int bq_snapshot_dump(bq_snapshot_t *snap){
    if (snap == NULL) return -1;

    bq_unseal();

    uint8_t zero = 0x00;
    uint8_t subclass = SUBCLASS_STATE;

    if (bq_write_bytes(REG_BLOCK_DATA_CONTROL, &zero, 1) != 1) return -1;
    if (bq_write_bytes(REG_DATA_CLASS, &subclass, 1) != 1) return -1;
    if (bq_write_bytes(REG_DATA_BLOCK, &zero, 1) != 1) return -1;
    vTaskDelay(pdMS_TO_TICKS(5));

    if (bq_read_bytes(REG_BLOCK_DATA, snap->state_block, 32) != 1) {
        bq_seal();
        return -1;
    }

    snap->valid = true;
    bq_seal();
    return 1;
}

int bq_snapshot_load(const bq_snapshot_t *snap){
    if (snap == NULL || !snap->valid) return -1;

    if (bq_enter_cfgupdate() != 1) {
        return -1;
    }

    uint8_t zero = 0x00;
    uint8_t subclass = SUBCLASS_STATE;

    bq_write_bytes(REG_BLOCK_DATA_CONTROL, &zero, 1);
    bq_write_bytes(REG_DATA_CLASS, &subclass, 1);
    bq_write_bytes(REG_DATA_BLOCK, &zero, 1);
    vTaskDelay(pdMS_TO_TICKS(5));

    if (bq_write_bytes(REG_BLOCK_DATA, snap->state_block, 32) != 1) {
        bq_exit_cfgupdate();
        return -1;
    }

    uint8_t csum = bq_calc_checksum(snap->state_block);
    bq_write_bytes(REG_BLOCK_DATA_CHECKSUM, &csum, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    return bq_exit_cfgupdate();
}

int bq_init(bq_config_t *config){
    /*
     * Initialization I2C device and BQ27441-G1 chip
     */
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = BQ_SDA_NUM,
        .scl_io_num = BQ_SCL_NUM,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = BQ_I2C_FREQ,
    };
    i2c_param_config(I2C_MASTER_PORT, &conf);
    i2c_driver_install(I2C_MASTER_PORT, conf.mode, 0, 0, 0);

    int flags = bq_reg_read(REG_FLAGS);
    if (flags == -1) {
        return -1;
    }

    /*  initialization and RAM configuration update if ITPOR is set or config provided  */
    if ((flags & FLAG_ITPOR) || config != NULL) {
        bq_snapshot_t snap;
        bq_snapshot_create_default(config, &snap);
        bq_snapshot_load(&snap);
    }

    return 1;
}

/**
 * Return battery voltage in mV.
 */
int bq_voltage(void){
    return bq_reg_read(REG_VOLTAGE);
}

/**
 * Return average current in mA (signed).
 */
int16_t bq_current(void){
    int val = bq_reg_read(REG_AVERAGE_CURRENT);
    if (val == -1) return 0;
    return (int16_t)val;
}

/**
 * Return state of charge in % (0..100).
 */
int bq_soc(void){
    return bq_reg_read(REG_STATE_OF_CHARGE);
}

/**
 * Return remaining capacity in mAh.
 */
int bq_remaining_capacity(void){
    return bq_reg_read(REG_REMAINING_CAPACITY);
}

/**
 * Return full charge capacity in mAh.
 */
int bq_full_capacity(void){
    return bq_reg_read(REG_FULL_CHARGE_CAPACITY);
}

/**
 * Return average power in mW (signed).
 */
int16_t bq_power(void){
    int val = bq_reg_read(REG_AVERAGE_POWER);
    if (val == -1) return 0;
    return (int16_t)val;
}

/**
 * Return battery temperature in Celsius.
 */
float bq_temperature(void){
    int val = bq_reg_read(REG_TEMPERATURE);
    if (val == -1) return 0.0f;
    return ((float)val * 0.1f) - 273.15f;
}

/**
 * Return state of health in %.
 */
int bq_soh(void){
    int val = bq_reg_read(REG_STATE_OF_HEALTH);
    if (val == -1) return -1;
    return (val & 0xFF);
}

void bq_reset(void){
    bq_unseal();
    bq_ctrl_write(CTRL_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(10));
    bq_seal();
}

void bq_sleep(void){
    bq_ctrl_write(CTRL_SET_HIBERNATE);
}

void bq_wakeup(void){
    bq_ctrl_write(CTRL_CLEAR_HIBERNATE);
}

/**
 * Shutdown hardware.
 */
void bq_close(void){
    bq_sleep();
    i2c_driver_delete(I2C_MASTER_PORT);
}
