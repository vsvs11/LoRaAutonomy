/*Данные файлы subsystem являются тестовыми,некими полигонами и нечего общего с основной частью в виде ядра и т.д не имеют*/
#pragma once

#include "driver/gpio.h"

#define BUTTON_PIN GPIO_NUM_15

//void IRAM_ATTR button_isr_handler(void *arg);

void power(void *pvParameters);//тестовая задача