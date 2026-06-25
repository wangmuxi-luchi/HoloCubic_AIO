/***************************************************
  HoloCubic多功能固件源码
  （项目中若参考本工程源码，请注明参考来源）

  聚合多种APP，内置天气、时钟、相册、特效动画、视频播放、视频投影、
  浏览器文件修改。（各APP具体使用参考说明书）

  Github repositories：https://github.com/ClimbSnail/HoloCubic_AIO

  Last review/edit by ClimbSnail: 2023/01/14
 ****************************************************/

#include "driver/lv_port_indev.h"
#include "driver/lv_port_fs.h"

#include "common.h"
#include "sys/app_controller.h"

#include "app/app_conf.h"

#include <SPIFFS.h>
#include <esp32-hal.h>
#include <esp32-hal-timer.h>
#include <FreeRTOS.h>
#include <task.h>

// 阶段2: 多任务重构
#include <FreeRTOS.h>
#include <task.h>
#include "network_async.h"

TaskHandle_t g_app_main_task_handle = NULL;
bool isCheckAction = false;

/*** Component objects **7*/
ImuAction *act_info;           // 存放mpu6050返回的数据
AppController *app_controller; // APP控制器

TimerHandle_t xTimerAction = NULL;
TaskHandle_t g_app_main_task_handle = NULL;
void actionCheckHandle(TimerHandle_t xTimer)
{
    // IMU 动作检测
    // 持锁检查：如果 act_info 已有有效动作（由 inject_action_direct 注入），
    // 则跳过本次定时器更新，避免覆盖已注入的动作
    bool skip_update = false;
    if (pdTRUE == xSemaphoreTake(g_action_mutex, portMAX_DELAY)) {
        skip_update = (act_info->active != UNKNOWN && act_info->isValid);
        xSemaphoreGive(g_action_mutex);
    }
    
    if (!skip_update) {
        act_info = mpu.getAction();
    }
    
    // 检查是否有新动作需要通知主循环（持锁读取，保证一致性）
    bool has_action = false;
    if (pdTRUE == xSemaphoreTake(g_action_mutex, portMAX_DELAY)) {
        has_action = (act_info->active != UNKNOWN && act_info->isValid);
        xSemaphoreGive(g_action_mutex);
    }
    
    if (has_action && g_app_main_task_handle) {
        xTaskNotifyGive(g_app_main_task_handle);
    }

    // 事件处理标志
    if (app_controller) {
        app_controller->set_event_deal_flag();
    }

    // WiFi 自动关闭检查（节能模式下超时则关闭 WiFi 并唤醒主循环）
    if (app_controller) {
        app_controller->wifi_auto_close_check();
    }
}

void my_print(const char *buf)
{
    Serial.printf("%s", buf);
    Serial.flush();
}

void setup()
{
    Serial.begin(115200);

    g_app_main_task_handle = xTaskGetCurrentTaskHandle();

    Serial.println(F("\nAIO (All in one) version " AIO_VERSION "\n"));
    Serial.flush();
    // MAC ID可用作芯片唯一标识
    Serial.print(F("ChipID(EfuseMac): "));
    Serial.println(ESP.getEfuseMac());
    // flash运行模式
    // Serial.print(F("FlashChipMode: "));
    // Serial.println(ESP.getFlashChipMode());
    // Serial.println(F("FlashChipMode value: FM_QIO = 0, FM_QOUT = 1, FM_DIO = 2, FM_DOUT = 3, FM_FAST_READ = 4, FM_SLOW_READ = 5, FM_UNKNOWN = 255"));
    Serial.printf("[SETUP] Step 1: Creating AppController...\n");
    Serial.flush();

    app_controller = new AppController(); // APP控制器
    Serial.printf("[SETUP] AppController created\n");
    Serial.flush();

    // 需要放在Setup里初始化
    Serial.printf("[SETUP] Step 2: Mounting SPIFFS...\n");
    Serial.flush();
    if (!SPIFFS.begin(true))
    {
        Serial.println("SPIFFS Mount Failed");
        return;
    }
    Serial.printf("[SETUP] SPIFFS OK\n");
    Serial.flush();

#ifdef PEAK
    pinMode(CONFIG_BAT_CHG_DET_PIN, INPUT);
    pinMode(CONFIG_ENCODER_PUSH_PIN, INPUT_PULLUP);
    /*电源使能保持*/
    Serial.println("Power: Waiting...");
    pinMode(CONFIG_POWER_EN_PIN, OUTPUT);
    digitalWrite(CONFIG_POWER_EN_PIN, LOW);
    digitalWrite(CONFIG_POWER_EN_PIN, HIGH);
    Serial.println("Power: ON");
    log_e("Power: ON");
#endif

    // config_read(NULL, &g_cfg);   // 旧的配置文件读取方式
    Serial.printf("[SETUP] Step 3: Reading config...\n");
    app_controller->read_config(&app_controller->sys_cfg);
    app_controller->read_config(&app_controller->mpu_cfg);
    app_controller->read_config(&app_controller->rgb_cfg);
    Serial.printf("[SETUP] Config OK\n");
    Serial.flush();

    /*** Init screen ***/
    Serial.printf("[SETUP] Step 4: screen.init()...\n");
    screen.init(app_controller->sys_cfg.rotation,
                app_controller->sys_cfg.backLight);
    Serial.printf("[SETUP] screen.init() OK\n");
    Serial.flush();

    /*** Init on-board RGB ***/
    rgb.init();
    rgb.setBrightness(0.05).setRGB(0, 64, 64);

    /*** Init ambient-light sensor ***/
    ambLight.init(ONE_TIME_H_RESOLUTION_MODE);

    /*** Init micro SD-Card ***/
    tf.init();

    lv_fs_fatfs_init();
    Serial.printf("[SETUP] Step 5: app_controller->init()...\n");
    Serial.flush();

#if LV_USE_LOG
    lv_log_register_print_cb(my_print);
#endif /*LV_USE_LOG*/

    app_controller->init();
    Serial.printf("[SETUP] app_controller->init() OK\n");
    Serial.flush();

    // 将APP"安装"到controller里
#if APP_WEATHER_USE
    app_controller->app_install(&weather_app);
#endif
#if APP_WEATHER_OLD_USE
    app_controller->app_install(&weather_old_app);
#endif
#if APP_TOMATO_USE
    app_controller->app_install(&tomato_app);
#endif
#if APP_PICTURE_USE
    app_controller->app_install(&picture_app);
#endif
#if APP_MEDIA_PLAYER_USE
    app_controller->app_install(&media_app);
#endif
#if APP_SCREEN_SHARE_USE
    app_controller->app_install(&screen_share_app);
#endif
#if APP_FILE_MANAGER_USE
    app_controller->app_install(&file_manager_app);
#endif

#if APP_WEB_SERVER_USE
    app_controller->app_install(&server_app);
#endif

#if APP_IDEA_ANIM_USE
    app_controller->app_install(&idea_app);
#endif
#if APP_BILIBILI_FANS_USE
    app_controller->app_install(&bilibili_app);
#endif
#if APP_SETTING_USE
    app_controller->app_install(&settings_app);
#endif
#if APP_GAME_2048_USE
    app_controller->app_install(&game_2048_app);
#endif
#if APP_GAME_SNAKE_USE
    app_controller->app_install(&game_snake_app);
#endif
#if APP_ANNIVERSARY_USE
    app_controller->app_install(&anniversary_app);
#endif
#if APP_HEARTBEAT_USE
    app_controller->app_install(&heartbeat_app, APP_TYPE_BACKGROUND);
#endif
#if APP_STOCK_MARKET_USE
    app_controller->app_install(&stockmarket_app);
#endif
#if APP_PC_RESOURCE_USE
    app_controller->app_install(&pc_resource_app);
#endif
#if APP_LHLXW_USE
    app_controller->app_install(&LHLXW_app);
#endif
    // 自启动APP
    app_controller->app_auto_start();

    // 优先显示屏幕 加快视觉上的开机时间
    Serial.printf("[SETUP] Step 6: First main_process()...\n");
    Serial.flush();
    app_controller->main_process(&mpu.action_info);
    Serial.printf("[SETUP] First main_process() OK\n");
    Serial.flush();

    /*** Init IMU as input device ***/
    // lv_port_indev_init();

    Serial.printf("[SETUP] Step 7: mpu.init()...\n");
    mpu.init(app_controller->sys_cfg.mpu_order,
             app_controller->sys_cfg.auto_calibration_mpu,
             &app_controller->mpu_cfg); // 初始化比较耗时
    Serial.printf("[SETUP] mpu.init() OK\n");

    /*** 以此作为MPU6050初始化完成的标志 ***/
    RgbConfig *rgb_cfg = &app_controller->rgb_cfg;
    // 初始化RGB灯 HSV色彩模式
    RgbParam rgb_setting = {LED_MODE_HSV,
                            rgb_cfg->min_value_0, rgb_cfg->min_value_1, rgb_cfg->min_value_2,
                            rgb_cfg->max_value_0, rgb_cfg->max_value_1, rgb_cfg->max_value_2,
                            rgb_cfg->step_0, rgb_cfg->step_1, rgb_cfg->step_2,
                            rgb_cfg->min_brightness, rgb_cfg->max_brightness,
                            rgb_cfg->brightness_step, rgb_cfg->time};
    // 运行RGB任务
    set_rgb_and_run(&rgb_setting, RUN_MODE_TASK);

    // 先初始化一次动作数据 防空指针
    act_info = mpu.getAction();
    // 定义一个统一定时器：IMU动作检测 + 事件处理标志 + WiFi自动关闭检查
    xTimerAction = xTimerCreate("Action Check",
                                200 / portTICK_PERIOD_MS,
                                pdTRUE, (void *)0, actionCheckHandle);
    xTimerStart(xTimerAction, 0);
    Serial.printf("[SETUP] ====== setup() COMPLETE ======\n");
}

void loop()
{
    static unsigned long loop_count = 0;
    loop_count++;
    if (loop_count % 1000 == 0) {
        Serial.printf("[LOOP] frame=%lu\n", loop_count);
    }

#ifdef PEAK
    // 适配稚晖君的PEAK
    if (!mpu.Encoder_GetIsPush())
    {
        Serial.println("mpu.Encoder_GetIsPush()1");
        delay(1000);
        if (!mpu.Encoder_GetIsPush())
        {
            Serial.println("mpu.Encoder_GetIsPush()2");
            // 适配Peak的关机功能
            digitalWrite(CONFIG_POWER_EN_PIN, LOW);
        }
    }
#endif
    // LVGL 渲染：排空队列，同时获取下次定时器到期时间
    uint32_t time_until_next;
    do {
        Serial.printf("[LOOP] lv_timer_handler()\n");
        time_until_next = lv_timer_handler();
    } while (time_until_next == 0);
    
    // 动作检测已移至 actionCheckHandle 定时器回调，主循环直接消费
    // 持锁拷贝 + 立即清除原始数据，防止同一动作被重复处理
    ImuAction action_copy;
    if (pdTRUE == xSemaphoreTake(g_action_mutex, portMAX_DELAY)) {
        action_copy = *act_info;
        act_info->active = ACTIVE_TYPE::UNKNOWN;
        act_info->isValid = 0;
        xSemaphoreGive(g_action_mutex);
    }
    app_controller->main_process(&action_copy); // 运行当前进程

    // LVGL 渲染：排空队列，同时获取下次定时器到期时间
    // uint32_t time_until_next;
    do {
        Serial.printf("[LOOP] lv_timer_handler()\n");
        time_until_next = lv_timer_handler();
    } while (time_until_next == 0);

    // 根据 LVGL 定时器状态 + APP 需求，智能决定阻塞时长
    // lv_timer_handler() 返回值：
    //   UINT32_MAX → 无活跃定时器，可以放心阻塞
    //   其他值     → 距离下次定时器到期的毫秒数
    int interval = app_controller->get_loop_interval_ms();
    TickType_t timeout;

    if (interval < 0) {
        // 无 APP 运行：仅当 LVGL 有定时器时短暂唤醒
        if (time_until_next == UINT32_MAX) {
            timeout = portMAX_DELAY;
        } else {
            timeout = pdMS_TO_TICKS(time_until_next);
        }
    } else {
        // APP 运行中：取 APP 间隔和 LVGL 间隔的较小值
        uint32_t app_ms = (uint32_t)interval;
        if (time_until_next != UINT32_MAX && time_until_next < app_ms) {
            timeout = pdMS_TO_TICKS(time_until_next);
        } else {
            timeout = pdMS_TO_TICKS(app_ms);
        }
    }
    Serial.printf("[LOOP] timeout=%d\n", timeout);

    ulTaskNotifyTake(pdTRUE, timeout);
    Serial.printf("[LOOP] ulTaskNotifyTake()\n");
}