#include <FreeRTOS.h>
#include <task.h>
#include "hal_native.h"
#include <stdio.h>
#include <stdlib.h>

extern void setup();
extern void loop();

static void app_main_task(void *pvParameters)
{
    (void)pvParameters;
    printf("[SIM] app_main_task started, SDL init...\n");
    fflush(stdout);
    hal_native_init();
    printf("[SIM] SDL ready, entering setup()...\n");
    fflush(stdout);
    setup();
    printf("[SIM] setup() complete, entering loop()...\n");
    fflush(stdout);
    for (;;) {
        loop();
        if (!hal_native_loop()) {
            printf("[SIM] SDL quit, exiting...\n");
            exit(0);
        }
        vTaskDelay(1);
    }
}

int main()
{
    BaseType_t ret;

    printf("[SIM] main() start\n");
    fflush(stdout);

    ret = xTaskCreate(app_main_task, "AppMain", 16 * 1024, NULL, 3, NULL);
    printf("[SIM] xTaskCreate(AppMain) returned %d\n", (int)ret);
    fflush(stdout);

    printf("[SIM] Starting FreeRTOS scheduler...\n");
    fflush(stdout);
    vTaskStartScheduler();
    printf("[SIM] FATAL: scheduler returned!\n");
    return 0;
}