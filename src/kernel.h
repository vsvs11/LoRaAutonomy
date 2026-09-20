#pragma once

void start();//стартовая функция

int create_task(char* name, void *func, uint32_t stack_depth, uint8_t priority);//создание задачи

int kill_task(void *func);//удаление задачи