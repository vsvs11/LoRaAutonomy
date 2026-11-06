#pragma once

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

typedef struct {
    char* name;
    int num;
    int priority;
} packet_queue_t;
int queue_add(packet_queue_t task);

void view_queue();
