#include <string.h>
#include "list.h"

list_tasks_t list[TASKS_MAX] = {0};//создание блока структуры list

static portMUX_TYPE kernel_spinlock = portMUX_INITIALIZER_UNLOCKED;
static uint8_t num = 0;//счет задач


int task_add(const char* name,TaskFunction_t func, uint32_t stack,UBaseType_t priority,TaskHandle_t out_handle, subsys_id_t subsys_id){//добавление задачи в список
    if (num >= TASKS_MAX){
        return -1;
    }
    list[num].name = name;
    list[num].func = func;
    list[num].stack_bytes = stack;
    list[num].priority = priority;
    list[num].handle = out_handle;
    list[num].subsys_id = subsys_id;
    num++;
    return 1;
    
}

int task_delete(TaskHandle_t out_handle){
    if (out_handle == NULL) {
        return -1;
    }
    portENTER_CRITICAL(&kernel_spinlock);
    int found_idx = -1;
    for (int i = 0; i < num; i++) {
        if (list[i].handle == out_handle) {
            found_idx = i;
            break;
        }
    }
    if (found_idx == -1) {
        portEXIT_CRITICAL(&kernel_spinlock);
        return -1;
    }
    for (int i = found_idx; i < num - 1; i++) {
        list[i] = list[i + 1];
    }
    memset(&list[num - 1], 0, sizeof(list_tasks_t));
    num--;
    portEXIT_CRITICAL(&kernel_spinlock);
    return 0;




}

list_tasks_t *list_get(void){//выгрузка списка задач 
    return list;
}