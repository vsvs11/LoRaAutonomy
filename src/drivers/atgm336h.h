#ifndef ATGM336H_H
#define ATGM336H_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int baud_rate;
    int nav_mode;       // 1: GPS, 2: BDS, 3: GPS+BDS, 4: GLONASS, 5: GPS+GLONASS, 7: ALL
    int update_rate_hz; // 1, 2, 5, 10 Hz
} atgm_config_t;

int atgm_init(atgm_config_t *config);

void atgm_send_cmd(const char *cmd);

int atgm_receive_packet(uint8_t *buf, int size);

int atgm_receive_line(char *buf, int max_size);

void atgm_set_mode(int nav_mode);

void atgm_set_update_rate(int rate_hz);

void atgm_set_baudrate(int baud);

void atgm_reset(int reset_type);

void atgm_sleep(void);

void atgm_wakeup(void);

void atgm_close(void);

#endif // ATGM336H_H
