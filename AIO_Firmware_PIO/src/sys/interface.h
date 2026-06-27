#ifndef INTERFACE_H
#define INTERFACE_H

enum APP_MESSAGE_TYPE
{
    APP_MESSAGE_WIFI_CONN = 0, // 开启连接
    APP_MESSAGE_WIFI_AP,       // 开启AP事件
    APP_MESSAGE_WIFI_ALIVE,    // wifi开关的心跳维持
    APP_MESSAGE_WIFI_DISCONN,  // 连接断开
    APP_MESSAGE_UPDATE_TIME,
    APP_MESSAGE_MQTT_DATA, // MQTT客户端收到消息
    APP_MESSAGE_GET_PARAM, // 获取参数
    APP_MESSAGE_SET_PARAM, // 设置参数
    APP_MESSAGE_READ_CFG,  // 向磁盘读取参数
    APP_MESSAGE_WRITE_CFG, // 向磁盘写入参数

    APP_MESSAGE_NONE
};

enum APP_TYPE
{
    APP_TYPE_REAL_TIME = 0, // 实时应用
    APP_TYPE_BACKGROUND,    // 后台应用

    APP_TYPE_NONE
};

class AppController;
struct ImuAction;

struct APP_OBJ
{
    // 应用程序名称 及title
    const char *app_name;

    // APP的图片存放地址    APP应用图标 128*128
    const void *app_image;

    // 应用程序的其他信息 如作者、版本号等等
    const char *app_info;

    // APP的初始化函数 也可以为空或什么都不做（作用等效于arduino setup()函数）
    int (*app_init)(AppController *sys);

    // APP的主程序函数入口指针
    void (*main_process)(AppController *sys,
                         const ImuAction *act_info);

    // APP的任务的入口指针（一般一分钟内会调用一次）
    void (*background_task)(AppController *sys,
                            const ImuAction *act_info);

    // 退出之前需要处理的回调函数 可为空
    int (*exit_callback)(void *param);

    // 消息处理机制
    void (*message_handle)(const char *from, const char *to,
                           APP_MESSAGE_TYPE type, void *message,
                           void *ext_info);

    // 主循环期望间隔（毫秒）
    //   0 (默认): 永久阻塞（等 IMU 动作 / 网络响应 / WiFi 超时通知唤醒）
    //   1~999: 按设定值定时唤醒
    // 需要多帧初始化的 APP：app_init() 中设为 >0，初始化完成后设回 0
    int loop_interval_ms;

    // 固定帧率模式：true → get_loop_interval_ms() 自动计算剩余阻塞时间
    //   false（默认）→ 直接使用 loop_interval_ms 原值
    bool fixed_fps_mode;

    // 下一帧预期时间戳（固定帧率模式用）
    //   帧到期时推进 last_frame_ms += loop_interval_ms，按键唤醒不重置
    unsigned long last_frame_ms;
};

#endif