#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t color;
    char text[16];
} display_cmd_t;

void display_subsys_init(void);
bool display_send_cmd(const display_cmd_t *cmd, uint32_t timeout_ms);