#pragma once

#include <stdio.h>
#include <stdint.h>

#define TASKS_MAX 15 //предел задач

struct list_tasks{
    //uint8_t num;
    char* name_task;
    void *pointer;
    size_t steck_depth;
    uint8_t priority;
};

void task_add(char* name,void *func,size_t steck,uint8_t priority);

struct list_tasks *list_get(void);
