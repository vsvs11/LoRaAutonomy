#include "config.h"
#include "esp_log.h"
#include "kernel.h"

int create_task(const char* name, TaskFunction_t func, void *arg, uint32_t stack_bytes, UBaseType_t priority, TaskHandle_t *out_handle, subsys_id_t subsys_id){
    if (func == NULL || name == NULL) {
    return KERNEL_STATUS_INVALID;
    }
    if (stack_bytes < 1024 || stack_bytes > 16384) {
        return KERNEL_STATUS_INVALID;
    }
    if (priority >= configMAX_PRIORITIES) {
        return KERNEL_STATUS_INVALID;
    }
    TaskHandle_t created_handle = NULL;
    BaseType_t res = xTaskCreate(
        func,
        name,
        stack_bytes,
        arg,
        priority,
        &created_handle   
    );
    if (res != pdPASS) {
        return KERNEL_STATUS_INVALID;
    }
    if (out_handle != NULL) {
        *out_handle = created_handle;
    }
    return KERNEL_STATUS_OK;
    
}
int kill_task(TaskHandle_t handle){
    if(handle == NULL){
        return -1;
    }
    vTaskDelete(handle);
    return 1;
    
}

int create_queue(UBaseType_t queue_len, UBaseType_t item_size, QueueHandle_t *out_queue) {
    if (out_queue == NULL || queue_len == 0 || item_size == 0) {
        return -1;
    }

    QueueHandle_t q = xQueueCreate(queue_len, item_size);
    if (q == NULL) {
        printf("falled create queue\n");
        return -1;
    }

    *out_queue = q;
    return 1;
}
