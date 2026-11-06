#include "lora_packet.h"
#include "lora_tasks.h"
#include "esp_log.h"

#define MAX_QUEUE           20

static const char *TAG = "TASK_PACKET";

packet_queue_t queue[MAX_QUEUE];
int ptr = 0;


int queue_add(packet_queue_t task){
    if (ptr >= MAX_QUEUE){
        return -1;
    }
    //packet_queue_t buf;
    /*for (int a = 0; a < MAX_QUEUE; a++){
        if (task.priority <= queue[a].priority){
            for (int b = 0; b < MAX_QUEUE-a-1; b++){
                buf = queue[a+b+1];
                queue[a+b+1] = queue[a+b];
                queue[a+b+2] = buf;    
            }
        }
    }*/
    queue[ptr].name = task.name;
    queue[ptr].num = task.num;
    queue[ptr].priority = task.priority;
    ptr++;
    return 0;
}

void view_queue(){
    for (int a = 0; a < 20; a++){
        ESP_LOGI(TAG, " (%d)  name - %s, num - %d, priority - %d", a, queue[a].name, queue[a].num, queue[a].priority);
    }
}