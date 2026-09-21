#pragma once

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define TASKS_MAX 15 //предел задач

typedef enum {
    SUBSYS_CORE = 0,    // Само ядро
    SUBSYS_POWER,       // Контроль батареи, зарядка, сон
    SUBSYS_LORA,        // Радиостек (RX, TX)
    SUBSYS_SENSORS      // Датчики
} subsys_id_t;

typedef struct list_all_task {
    const char* name;
    TaskFunction_t func;
    uint32_t stack_bytes;
    UBaseType_t priority;
    TaskHandle_t handle;
    subsys_id_t subsys_id;

} list_tasks_t;//структура таблицы задач

int task_add(const char* name,TaskFunction_t func,
            uint32_t stack_bytes, UBaseType_t priority, 
            TaskHandle_t out_handle, subsys_id_t subsys_id);//добавление задачи в таблицу

int task_delete(TaskHandle_t out_handle);//удаление задачи из таблицы

list_tasks_t *list_get(void);//передача таблицы 


