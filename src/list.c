#include "list.h"

struct list_tasks list[TASKS_MAX] = {0};//создание блока структуры list

static uint8_t num = 0;//счет задач


void task_add(char* name,void *func,size_t steck,uint8_t priority){//добавление задачи в список
    list[num].name_task = name;
    list[num].pointer = func;
    list[num].steck_depth = steck;
    list[num].priority = priority;
    num++;
    //добавить защиту на переполнение
}

void task_delete(void *func){//удаление задачи по адресу/имени/номеру из списка
    //находим номер задачи по адресу или названию,и сносим все последующие задачи в списке на -1 для выравнивания
}

struct list_tasks *list_get(void){//выгрузка списка задач 
    return list;
}