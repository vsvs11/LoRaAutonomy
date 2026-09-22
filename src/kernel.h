#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_timer.h"
#include "freertos/semphr.h"
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include "list.h"

void sys_heartbeat(subsys_id_t subsys);//рапорт о работе

int create_task(const char* name, TaskFunction_t func,void *arg,
                uint32_t stack_bytes, UBaseType_t priority,
                TaskHandle_t *out_handle, subsys_id_t subsys_id);//создание задачи

int kill_task(TaskHandle_t handle);//уничтожение задачи

int kill_subsystem(subsys_id_t subsys_id);//уничтожение подсистемы

void start();//стартовая функция

void supervisor(void *pvParameters);//главная задача супервизора


int create_queue(UBaseType_t len_queue, UBaseType_t item_size,
                QueueHandle_t *out_queue);//создание очереди