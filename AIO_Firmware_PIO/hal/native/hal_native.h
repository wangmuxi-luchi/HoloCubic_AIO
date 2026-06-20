#ifndef HAL_NATIVE_H
#define HAL_NATIVE_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

#ifdef __cplusplus
#include "MPU6050.h"
extern "C" {
#endif

// 初始化所有 HAL 模拟层（SDL 窗口、输入设备等）
void hal_native_init(void);

// 主循环：处理 SDL 事件 + LVGL 定时器（单次迭代，返回 false 表示退出）
bool hal_native_loop(void);

// 清理 SDL 资源
void hal_native_cleanup(void);

// 显示 flush 回调：将 LVGL 帧buffer渲染到 SDL2 窗口
void hal_display_flush(int x1, int y1, int x2, int y2, const void *color_p);

// LVGL 显示刷新回调（符合 LVGL v8 标准签名）
void hal_lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p);

// IMU 模拟：获取键盘生成的模拟动作数据
// 返回: 0=无动作, 1=返回, 2=左翻, 3=右翻, 4=上, 5=下, 6=摇晃, 7=前倾/进入
int hal_imu_get_action(void);

// 获取 IMU 原始数据（加速度 + 陀螺仪）
void hal_imu_get_motion(int16_t *ax, int16_t *ay, int16_t *az,
                        int16_t *gx, int16_t *gy, int16_t *gz);

// 环境光传感器：返回模拟光照值（lux）
unsigned int hal_ambient_get_lux(void);

// SD 卡模拟初始化
bool hal_sd_init(void);

// SD 卡文件操作：读取文件
bool hal_sd_read_file(const char *path, uint8_t *buf, size_t *len);

// SD 卡文件操作：写入文件
bool hal_sd_write_file(const char *path, const uint8_t *buf, size_t len);

// SD 卡文件操作：检查文件是否存在
bool hal_sd_exists(const char *path);

// RGB LED 模拟
void hal_rgb_set_color(uint8_t r, uint8_t g, uint8_t b);

// 自动测试：向模拟器注入动作（替代键盘输入）
void hal_native_inject_action(int action);

// 自动测试钩子（弱函数，由 auto_test.cpp 覆盖）
// 在 hal_native_loop() 中每帧调用，可在此注入测试动作
void hal_native_auto_test_hook(void);

// 检查 JPEG 解码是否完成（由 TJpg_Decoder 设置，自动测试框架读取）
// 返回 1 表示自上次查询以来有新的解码完成，读取后自动清零
int hal_native_is_jpg_decode_done(void);

// 通知 auto_test 框架：服务器路由已注册完毕
// 由 server.cpp 在 start_web_config() 之后调用
void hal_native_server_routes_ready(void);

#ifdef __cplusplus
}
#endif

#endif // HAL_NATIVE_H