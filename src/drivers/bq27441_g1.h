#ifndef BQ27441_G1_H
#define BQ27441_G1_H

#include <stdint.h>

int bq_reg_write(uint8_t reg, uint16_t *data, size_t size);
int bq_reg_read(uint8_t reg);
void bq_init(void);

#endif // BQ27441_G1_H
