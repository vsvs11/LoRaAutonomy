#include "freertos/idf_additions.h"
#include "kernel.h"
#include "subsystems.h"
#include "freertos/FreeRTOS.h"
#include <stdio.h> 

TaskHandle_t *handle;

void app_main(void) {
    int err = create_task("packet_task", packet_task, NULL, 4096, 15, handle, SUBSYS_CORE);
    printf("%d", err);
}
