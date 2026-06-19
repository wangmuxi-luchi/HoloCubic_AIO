#include "common.h"
#include "hal_native.h"
#include <cstdio>
#include <cstring>

// ==================== Display ====================
void Display::init(uint8_t rotation, uint8_t backLight)
{
    (void)rotation;
    (void)backLight;

    printf("[DISP] lv_init()...\n");
    lv_init();

    static lv_color_t buf[SCREEN_HOR_RES * 24];
    static lv_disp_draw_buf_t draw_buf;
    lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_HOR_RES * 24);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_HOR_RES;
    disp_drv.ver_res = SCREEN_VER_RES;
    disp_drv.flush_cb = hal_lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    printf("[DISP] LVGL initialized, driver registered (240x240)\n");
}

void Display::routine()
{
    static unsigned long routine_count = 0;
    routine_count++;
    if (routine_count % 100 == 0) {
        printf("[DISP] routine() frame=%lu\n", routine_count);
    }
    AIO_LVGL_OPERATE_LOCK(lv_timer_handler());
}

void Display::setBackLight(float val)
{
    (void)val;
}

// ==================== Pixel ====================
void Pixel::init()
{
}

Pixel& Pixel::setRGB(int r, int g, int b)
{
    (void)r;
    (void)g;
    (void)b;
    return *this;
}

Pixel& Pixel::setHVS(uint8_t ih, uint8_t is, uint8_t iv)
{
    (void)ih;
    (void)is;
    (void)iv;
    return *this;
}

Pixel& Pixel::fill_rainbow(int min_r, int max_r, int min_g, int max_g, int min_b, int max_b)
{
    (void)min_r;
    (void)max_r;
    (void)min_g;
    (void)max_g;
    (void)min_b;
    (void)max_b;
    return *this;
}

Pixel& Pixel::setBrightness(float duty)
{
    (void)duty;
    return *this;
}

// ==================== Ambient ====================
void Ambient::init(int mode)
{
    (void)mode;
}

// ==================== SdCard ====================
void SdCard::init()
{
}

// ==================== IMU ====================
IMU::IMU()
{
}

void IMU::init(uint8_t order, uint8_t auto_calibration, SysMpuConfig *mpu_cfg)
{
    (void)order;
    (void)auto_calibration;
    (void)mpu_cfg;
}

void IMU::setOrder(uint8_t order)
{
    (void)order;
}

bool IMU::Encoder_GetIsPush(void)
{
    return false;
}

ImuAction* IMU::getAction(void)
{
    return &action_info;
}

void IMU::getVirtureMotion6(ImuAction *action_info)
{
    (void)action_info;
}

// ==================== FlashFS ====================
FlashFS::FlashFS()
{
}

FlashFS::~FlashFS()
{
}

uint16_t FlashFS::readFile(const char *path, uint8_t *info)
{
    (void)path;
    if (info) {
        info[0] = '\0';
    }
    return 0;
}

void FlashFS::writeFile(const char *path, const char *message)
{
    (void)path;
    (void)message;
}

void FlashFS::listDir(const char *dirname, uint8_t levels)
{
    (void)dirname;
    (void)levels;
}

void FlashFS::appendFile(const char *path, const char *message)
{
    (void)path;
    (void)message;
}

void FlashFS::renameFile(const char *src, const char *dst)
{
    (void)src;
    (void)dst;
}

void FlashFS::deleteFile(const char *path)
{
    (void)path;
}

// ==================== Network ====================
Network::Network()
{
}

void Network::search_wifi(void)
{
}

boolean Network::start_conn_wifi(const char *ssid, const char *password)
{
    (void)ssid;
    (void)password;
    return true;
}

boolean Network::end_conn_wifi(void)
{
    return true;
}

boolean Network::close_wifi(void)
{
    return true;
}

boolean Network::open_ap(const char *ap_ssid, const char *ap_password)
{
    (void)ap_ssid;
    (void)ap_password;
    return true;
}

// ==================== 全局函数 ====================
bool set_rgb_and_run(RgbParam *rgb_setting, LED_RUN_MODE mode)
{
    (void)rgb_setting;
    (void)mode;
    return true;
}

void rgb_stop(void)
{
}

bool analyseParam(char *info, int argc, char **argv)
{
    (void)info;
    (void)argc;
    (void)argv;
    return true;
}

// ==================== 全局变量 ====================
const char *AP_SSID = "HoloCubic_AIO";

const char *active_type_info[] = {
    "TURN_RIGHT",
    "RETURN",
    "TURN_LEFT",
    "UP",
    "DOWN",
    "GO_FORWORD",
    "SHAKE",
    "UNKNOWN"
};

// ==================== FreeRTOS 回调 ====================
extern "C" void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                              StackType_t **ppxIdleTaskStackBuffer,
                                              configSTACK_DEPTH_TYPE *puxIdleTaskStackSize)
{
    static StaticTask_t xIdleTaskTCB;
    static StackType_t uxIdleTaskStack[configMINIMAL_STACK_SIZE];
    *ppxIdleTaskTCBBuffer = &xIdleTaskTCB;
    *ppxIdleTaskStackBuffer = uxIdleTaskStack;
    *puxIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

extern "C" void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                               StackType_t **ppxTimerTaskStackBuffer,
                                               configSTACK_DEPTH_TYPE *pulTimerTaskStackSize)
{
    static StaticTask_t xTimerTaskTCB;
    static StackType_t uxTimerTaskStack[configTIMER_TASK_STACK_DEPTH];
    *ppxTimerTaskTCBBuffer = &xTimerTaskTCB;
    *ppxTimerTaskStackBuffer = uxTimerTaskStack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}