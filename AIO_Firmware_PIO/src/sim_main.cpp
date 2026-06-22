#include <FreeRTOS.h>
#include <task.h>
#include "hal_native.h"
#include "auto_test.h"
#include "log_buffer.h"
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

extern void setup();
extern void loop();

bool g_system_ready = false;

extern TaskHandle_t g_app_main_task_handle;

static void log_printf(const char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    if (len > 0) {
        log_buffer_write(buf, (uint32_t)len);
    }
}

// SDL 线程入口（Windows 线程，非 FreeRTOS 任务）
static DWORD WINAPI sdl_thread_entry(LPVOID param)
{
    (void)param;
    hal_native_sdl_thread();
    return 0;
}

static void app_main_task(void *pvParameters)
{
    (void)pvParameters;
    log_printf("[SIM] app_main_task started, entering setup()...\n");
    setup();
    g_system_ready = true;
    log_printf("[SIM] setup() complete, entering AppCtrl loop...\n");
    for (;;) {
        loop();
        hal_native_loop();
        vTaskDelay(1);
    }
}

int main(int argc, char *argv[])
{
    BaseType_t ret;

    auto_test_init(argc, argv);

    printf("[SIM] main() start\n");
    fflush(stdout);

    log_buffer_init();

    // 创建 SDL 线程（窗口 + 事件 + 渲染 + auto_test）
    // 对应真实硬件中的 SPI 总线 + GPIO 中断，独立于 APP 逻辑运行
    HANDLE hSdlThread = CreateThread(NULL, 0, sdl_thread_entry, NULL, 0, NULL);
    if (!hSdlThread) {
        printf("[SIM] FATAL: Failed to create SDL thread!\n");
        return 1;
    }

    // 等待 SDL 线程完成窗口初始化
    printf("[SIM] Waiting for SDL thread to initialize...\n");
    fflush(stdout);
    while (!hal_native_is_ready()) {
        Sleep(10);
    }
    printf("[SIM] SDL thread ready, starting FreeRTOS scheduler...\n");
    fflush(stdout);

    ret = xTaskCreate(app_main_task, "AppMain", 16 * 1024, NULL, 3, NULL);
    printf("[SIM] xTaskCreate(AppMain) returned %d\n", (int)ret);
    fflush(stdout);

    printf("[SIM] Starting FreeRTOS scheduler...\n");
    fflush(stdout);
    vTaskStartScheduler();
    printf("[SIM] FATAL: scheduler returned!\n");
    log_buffer_shutdown();
    return 0;
}