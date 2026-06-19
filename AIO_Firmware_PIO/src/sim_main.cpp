#include <FreeRTOS.h>
#include <task.h>
#include "hal_native.h"
#include <stdio.h>

extern void setup();
extern void loop();

static void app_main_task(void *pvParameters)
{
    (void)pvParameters;
    printf("[SIM] app_main_task started, entering setup()...\n");
    fflush(stdout);
    setup();
    printf("[SIM] setup() complete, entering loop()...\n");
    fflush(stdout);
    for (;;) {
        loop();
        vTaskDelay(5);
    }
}

static void sdl_event_task(void *pvParameters)
{
    (void)pvParameters;
    printf("[SIM] sdl_event_task started\n");
    fflush(stdout);
    for (;;) {
        hal_native_loop();
        vTaskDelay(1);
    }
}

int main()
{
    BaseType_t ret;

    printf("[SIM] main() start\n");
    fflush(stdout);
    hal_native_init();

    ret = xTaskCreate(app_main_task, "AppMain", 16 * 1024, NULL, 3, NULL);
    printf("[SIM] xTaskCreate(AppMain) returned %d\n", (int)ret);
    fflush(stdout);

    ret = xTaskCreate(sdl_event_task, "SDLEvent", 4 * 1024, NULL, 1, NULL);
    printf("[SIM] xTaskCreate(SDLEvent) returned %d\n", (int)ret);
    fflush(stdout);

    printf("[SIM] Starting FreeRTOS scheduler...\n");
    fflush(stdout);
    vTaskStartScheduler();
    printf("[SIM] FATAL: scheduler returned!\n");
    return 0;
}