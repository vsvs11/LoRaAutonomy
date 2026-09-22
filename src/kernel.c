#include "esp_log.h"
#include "kernel.h"
#include "subsystems.h"

#define SUBSYS_COUNT 4
#define HEARTBEAT_TIMEOUT_MS 5000


typedef struct {
    uint64_t last_seen_ms;
    bool is_monitored;
} subsys_health_t;//структура жизненного состаяния подсистемы

static const char *TAG_KERNEL = "KERNEL";

// 2. Спинлок ядра для синхронизации в SMP (ESP32-S3)
static portMUX_TYPE kernel_spinlock = portMUX_INITIALIZER_UNLOCKED;

static subsys_health_t subsys_health[SUBSYS_COUNT] = {0};

void sys_heartbeat(subsys_id_t subsys){
    if (subsys <= SUBSYS_CORE || subsys >= SUBSYS_COUNT) {
        return;
    }

    uint64_t now_ms = esp_timer_get_time() / 1000;

    portENTER_CRITICAL(&kernel_spinlock);
    subsys_health[subsys].last_seen_ms = now_ms;
    subsys_health[subsys].is_monitored = true;
    portEXIT_CRITICAL(&kernel_spinlock);
}

int create_task(const char* name, TaskFunction_t func, void *arg, uint32_t stack_bytes, UBaseType_t priority, TaskHandle_t *out_handle, subsys_id_t subsys_id){
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
        return -1;
    }
    int status = task_add(name, func, stack_bytes, priority, created_handle, subsys_id);
    if (status == -1){
        vTaskDelete(created_handle);
        return -1;
    }
    if (out_handle != NULL) {
        *out_handle = created_handle;
    }
    return 1;
    
}
int kill_task(TaskHandle_t handle){
    if(handle == NULL){
        return -1;
    }
    int status = task_delete(handle);
    if (status == -1){
        return -1;
    }
    vTaskDelete(handle);
    return 1;
    
}
int kill_subsystem(subsys_id_t subsystem_id){
    if (subsystem_id == SUBSYS_CORE){
        return -1;
    }
    TaskHandle_t targets[TASKS_MAX];
    int count = 0;
    list_tasks_t *tasks = list_get();
    for (int i = 0;i < TASKS_MAX;i++){
        if (tasks[i].handle != NULL && tasks[i].subsys_id == subsystem_id){
            targets[count++] = tasks[i].handle;
        }
    }
    for (int i = 0;i < count;i++){
        kill_task(targets[i]);
    }
    return count;
}

void start(void){//возможно перемешение стартовой функции в main.c
 
}

void supervisor(void *pvParameters){
    (void)pvParameters;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        uint64_t now_ms = esp_timer_get_time() / 1000;

        for (int i = 1; i < SUBSYS_COUNT; i++) {
            portENTER_CRITICAL(&kernel_spinlock);
            bool monitored = subsys_health[i].is_monitored;
            uint64_t last = subsys_health[i].last_seen_ms;
            portEXIT_CRITICAL(&kernel_spinlock);

            if (monitored && (now_ms - last > HEARTBEAT_TIMEOUT_MS)) {
                ESP_LOGI(TAG_KERNEL, "Subsystem %d hung! Heartbeat timed out (%llu ms). Killing...", 
                         i, now_ms - last);

                // Отключаем мониторинг, чтобы не спамить повторным киллом
                portENTER_CRITICAL(&kernel_spinlock);
                subsys_health[i].is_monitored = false;
                portEXIT_CRITICAL(&kernel_spinlock);

                // Прибиваем все дочерние таски зависшего модуля
                kill_subsystem((subsys_id_t)i);
            }
        }
    }

}

int create_queue(UBaseType_t queue_len, UBaseType_t item_size, QueueHandle_t *out_queue) {
    if (out_queue == NULL || queue_len == 0 || item_size == 0) {
        return -1;
    }

    QueueHandle_t q = xQueueCreate(queue_len, item_size);
    if (q == NULL) {
        ESP_LOGE(TAG_KERNEL, "Failed to allocate FreeRTOS queue");
        return -1;
    }

    *out_queue = q;
    return 1;
}
