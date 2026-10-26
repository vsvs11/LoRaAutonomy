#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
    uint8_t len;
    uint8_t id_recipient;
    uint8_t data[];
} packet_queue_t;
