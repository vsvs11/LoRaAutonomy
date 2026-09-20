#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "kernel.h"
#include "subsystems.h"
#include "list.h"


void start(){//возможно перемешение стартовой функции в отдельный файл
    task_add("power_1",power,2048,5);
    xTaskCreate(power, "power_1", 2048, NULL, 5, NULL);
    vTaskDelete(NULL);
}

int create_task(char* name, void *func, uint32_t stack_depth, uint8_t priority){
    task_add(name, func, stack_depth, priority);
    xTaskCreate(func, name, stack_depth, NULL, priority, NULL);
    return 1;
}
int kill_task(void *func){
    vTaskDelete(func);//переделать,надо передавать указатель функции через хэндл задач
    return 1;
}
/*   work priorities API
    0-2 = tasks kernel
    3-7 = tasks powerment  неверно,требуется переделать
    8-13 = tasks loranet
    14-25 = other tasks*/