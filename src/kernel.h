#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "list.h"

typedef enum {
    KERNEL_STATUS_OK = 0,
    KERNEL_STATUS_INVALID = -1,
    KERNEL_STATUS_UNKOWN = -2
    
}kernel_err_t;

int create_task(const char* name, TaskFunction_t func,void *arg,
                uint32_t stack_bytes, UBaseType_t priority,
                TaskHandle_t *out_handle, subsys_id_t subsys_id);//создание задачи

int kill_task(TaskHandle_t handle);//уничтожение задачи

int create_queue(UBaseType_t len_queue, UBaseType_t item_size,
                QueueHandle_t *out_queue);//создание очереди