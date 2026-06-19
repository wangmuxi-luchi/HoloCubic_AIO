#include "hal_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>

// LVGL 头文件 - 在编译时由 PlatformIO 的 lib 路径提供
#include "lvgl.h"

// 屏幕分辨率（与 ESP32 硬件一致）
#define SDL_HOR_RES 240
#define SDL_VER_RES 240

static SDL_Window   *window   = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture  *texture  = NULL;
static lv_color_t   *fb_buf   = NULL;  // 整帧 framebuffer

// 键盘动作共享变量
typedef enum {
    HAL_ACTION_NONE = 0,
    HAL_ACTION_RETURN,
    HAL_ACTION_TURN_LEFT,
    HAL_ACTION_TURN_RIGHT,
    HAL_ACTION_UP,
    HAL_ACTION_DOWN,
    HAL_ACTION_SHAKE,
    HAL_ACTION_GO_FORWORD
} hal_action_t;

static volatile hal_action_t g_last_action = HAL_ACTION_NONE;

// tick 线程 - LV_TICK_CUSTOM=1 时由 millis() 提供 tick，不需要 lv_tick_inc()
static int tick_thread(void *data)
{
    (void)data;
    while (1) {
        SDL_Delay(5);
    }
    return 0;
}

void hal_native_init(void)
{
    SDL_Init(SDL_INIT_VIDEO);

    window = SDL_CreateWindow("HoloCubic_AIO Simulator",
                              SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                              SDL_HOR_RES * 2, SDL_VER_RES * 2,
                              SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_RGB565,
                                SDL_TEXTUREACCESS_STREAMING,
                                SDL_HOR_RES, SDL_VER_RES);

    fb_buf = (lv_color_t *)malloc(SDL_HOR_RES * SDL_VER_RES * sizeof(lv_color_t));
    memset(fb_buf, 0, SDL_HOR_RES * SDL_VER_RES * sizeof(lv_color_t));

    SDL_CreateThread(tick_thread, "tick", NULL);

    printf("[HAL] SDL2 initialized: %dx%d window\n", SDL_HOR_RES, SDL_VER_RES);
    printf("[HAL] Keyboard: LEFT/RIGHT=switch, UP/DOWN=vertical, SPACE=enter, BACKSPACE=back, ENTER=shake\n");
}

bool hal_native_loop(void)
{
    static unsigned long sdl_frame = 0;
    SDL_Event event;

    // 自动测试钩子：如果 auto_test.cpp 覆盖了此弱函数，
    // 则在此注入测试动作（替代键盘输入）
    hal_native_auto_test_hook();

    // 处理所有等待中的 SDL 事件
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            return false;
        }
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_LEFT:     g_last_action = HAL_ACTION_TURN_LEFT;  break;
                case SDLK_RIGHT:    g_last_action = HAL_ACTION_TURN_RIGHT; break;
                case SDLK_UP:       g_last_action = HAL_ACTION_UP;         break;
                case SDLK_DOWN:     g_last_action = HAL_ACTION_DOWN;       break;
                case SDLK_SPACE:    g_last_action = HAL_ACTION_GO_FORWORD; break;
                case SDLK_RETURN:   g_last_action = HAL_ACTION_SHAKE;      break;
                case SDLK_BACKSPACE: g_last_action = HAL_ACTION_RETURN;    break;
                default: break;
            }
        }
    }

    // 渲染 framebuffer 到 SDL 窗口
    SDL_UpdateTexture(texture, NULL, fb_buf, SDL_HOR_RES * sizeof(lv_color_t));
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);

    sdl_frame++;
    // 不在运行时刷屏，按需可改为 % 1000 或取消注释
    // if (sdl_frame % 100 == 0) {
    //     printf("[SDL] render frame=%lu\n", sdl_frame);
    // }

    SDL_Delay(1);
    return true;
}

void hal_native_cleanup(void)
{
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    free(fb_buf);
    SDL_Quit();
}

void hal_display_flush(int x1, int y1, int x2, int y2, const void *color_p)
{
    uint32_t w = (x2 - x1 + 1);
    uint32_t h = (y2 - y1 + 1);
    const lv_color_t *src = (const lv_color_t *)color_p;

    for (uint32_t y = 0; y < h; y++) {
        uint32_t dst_offset = (y1 + y) * SDL_HOR_RES + x1;
        uint32_t src_offset = y * w;
        memcpy(&fb_buf[dst_offset], &src[src_offset], w * sizeof(lv_color_t));
    }
}

void hal_lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    for (uint32_t y = 0; y < h; y++) {
        uint32_t dst_offset = (area->y1 + y) * SDL_HOR_RES + area->x1;
        uint32_t src_offset = y * w;
        memcpy(&fb_buf[dst_offset], &color_p[src_offset], w * sizeof(lv_color_t));
    }

    lv_disp_flush_ready(disp);
}

// 供 hal_imu.c 调用的动作获取函数
int hal_native_get_key_action(void)
{
    hal_action_t action = (hal_action_t)g_last_action;
    g_last_action = HAL_ACTION_NONE;  // 消费后清除
    return (int)action;
}

// 自动测试：向模拟器注入动作
void hal_native_inject_action(int action)
{
    g_last_action = (hal_action_t)action;
}

// 自动测试钩子（弱函数，默认空实现）
// 由 auto_test.cpp 覆盖以实现自动测试
__attribute__((weak)) void hal_native_auto_test_hook(void)
{
}