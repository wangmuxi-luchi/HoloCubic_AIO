#include "auto_test.h"
#include "hal_native.h"
#include "sys/app_controller.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

// 动作类型枚举，与 hal_display.c 中的 hal_action_t 保持一致
enum {
    AUTO_ACTION_NONE = 0,
    AUTO_ACTION_RETURN,
    AUTO_ACTION_TURN_LEFT,
    AUTO_ACTION_TURN_RIGHT,
    AUTO_ACTION_UP,
    AUTO_ACTION_DOWN,
    AUTO_ACTION_SHAKE,
    AUTO_ACTION_GO_FORWORD
};

// 钩子类型：定义每个测试步骤等待的完成信号
enum HookType {
    HOOK_NONE = 0,      // 无钩子：使用 frame_delay 帧计数等待
    HOOK_NAV,           // 导航钩子：等待 cur_app_index 变化
    HOOK_ENTER,         // 进入钩子：等待 app_exit_flag 变为 1
    HOOK_EXIT,          // 退出钩子：等待 app_exit_flag 变为 0
    HOOK_LOADING,       // 加载钩子：等待 JPG 解码完成，frame_delay 为超时
};

// isCheckAction 的外部声明（HoloCubic_AIO.cpp 中定义）
extern bool isCheckAction;

// app_controller 全局指针（HoloCubic_AIO.cpp 中定义）
extern AppController *app_controller;

// ========================
// 统一日志系统
// ========================

#define AUTO_TEST_TAG "AUTO_TEST"

static void get_timestamp(char *buf, size_t bufsize)
{
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, bufsize, "%Y-%m-%d %H:%M:%S", tm_info);
}

#define LOG_DEBUG(tag, fmt, ...) do { \
    char _ts[20]; get_timestamp(_ts, sizeof(_ts)); \
    printf("[%s] [DEBUG] [%s] %s:%d: " fmt "\n", _ts, tag, __FILE__, __LINE__, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)

#define LOG_INFO(tag, fmt, ...) do { \
    char _ts[20]; get_timestamp(_ts, sizeof(_ts)); \
    printf("[%s] [INFO]  [%s] %s:%d: " fmt "\n", _ts, tag, __FILE__, __LINE__, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)

#define LOG_WARN(tag, fmt, ...) do { \
    char _ts[20]; get_timestamp(_ts, sizeof(_ts)); \
    printf("[%s] [WARN]  [%s] %s:%d: " fmt "\n", _ts, tag, __FILE__, __LINE__, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)

#define LOG_ERROR(tag, fmt, ...) do { \
    char _ts[20]; get_timestamp(_ts, sizeof(_ts)); \
    printf("[%s] [ERROR] [%s] %s:%d: " fmt "\n", _ts, tag, __FILE__, __LINE__, ##__VA_ARGS__); \
    fflush(stdout); \
} while(0)

// ========================
// 测试步骤与用例结构
// ========================

struct TestStep {
    int frame_delay;   // 最大等待帧数 / 超时帧数（约 1ms/帧）
    int action;        // 注入的动作
    int hook_type;     // 等待的完成信号类型（HookType 枚举）
};

struct TestCase {
    const char *app_name;           // APP 名称
    int nav_steps;                  // 导航步数（备用，实际由动态计算覆盖）
    const TestStep *steps;          // 测试步骤数组
    int step_count;                 // 步骤数量
    int post_enter_delay;           // 进入 APP 后等待帧数（备用）
};

// ========================
// 测试状态
// ========================

static struct {
    bool running;
    bool inited;
    bool completed;                 // 测试已完成，不再重复执行
    const char *target_app;         // 目标 APP 名称，NULL 表示全部测试
    int current_step;
    int frame_counter;
    int phase;                      // 0=导航, 1=进入, 2=测试, 3=完成, 4=退出
    int nav_counter;
    int dynamic_nav_steps;          // 动态计算的导航步数
    int target_app_index;           // 目标 APP 在 appList 中的索引
    const TestCase *current_case;
    bool case_result;
    int total_tests;
    int passed_tests;

    // 钩子状态追踪
    int prev_app_index;             // 上一帧的 cur_app_index（用于 HOOK_NAV）
    int prev_enter_flag;            // 上一帧的 app_exit_flag（用于 HOOK_ENTER/HOOK_EXIT）
    bool nav_first_action_sent;     // 导航阶段是否已发送第一个动作
    bool enter_go_sent;             // 进入阶段是否已发送 GO_FORWORD
} test_state = {
    false, false, false, NULL, 0, 0, 0, 0, 0, -1, NULL, false, 0, 0,
    0, 0, false, false
};

// ========================
// 测试用例定义
// ========================

// Picture APP 测试
static const TestStep picture_steps[] = {
    {500,  AUTO_ACTION_NONE,       HOOK_LOADING},   // 等待图片加载（超时500帧）
    {2,    AUTO_ACTION_TURN_RIGHT, HOOK_NONE},      // 下一张
    {500,  AUTO_ACTION_NONE,       HOOK_LOADING},   // 等待加载
    {2,    AUTO_ACTION_TURN_RIGHT, HOOK_NONE},      // 下一张
    {500,  AUTO_ACTION_NONE,       HOOK_LOADING},   // 等待加载
    {2,    AUTO_ACTION_TURN_LEFT,  HOOK_NONE},      // 上一张
    {500,  AUTO_ACTION_NONE,       HOOK_LOADING},   // 等待加载
    {2,    AUTO_ACTION_RETURN,     HOOK_NONE},      // 退出
    {3000, AUTO_ACTION_NONE,       HOOK_EXIT},      // 等待退出（超时3000帧）
};

// Game 2048 APP 测试
static const TestStep game_2048_steps[] = {
    {2000, AUTO_ACTION_NONE,       HOOK_ENTER},     // 等待游戏加载（复用 HOOK_ENTER 检查 app_exit_flag）
    {2,    AUTO_ACTION_UP,         HOOK_NONE},      // 上移
    {30,   AUTO_ACTION_NONE,       HOOK_NONE},
    {2,    AUTO_ACTION_TURN_LEFT,  HOOK_NONE},      // 左移
    {30,   AUTO_ACTION_NONE,       HOOK_NONE},
    {2,    AUTO_ACTION_DOWN,       HOOK_NONE},      // 下移
    {30,   AUTO_ACTION_NONE,       HOOK_NONE},
    {2,    AUTO_ACTION_TURN_RIGHT, HOOK_NONE},      // 右移
    {30,   AUTO_ACTION_NONE,       HOOK_NONE},
    {2,    AUTO_ACTION_RETURN,     HOOK_NONE},      // 退出
    {3000, AUTO_ACTION_NONE,       HOOK_EXIT},      // 等待退出
};

// Settings APP 测试
static const TestStep settings_steps[] = {
    {2000, AUTO_ACTION_NONE,       HOOK_ENTER},     // 等待加载
    {2,    AUTO_ACTION_DOWN,       HOOK_NONE},      // 下移选项
    {20,   AUTO_ACTION_NONE,       HOOK_NONE},
    {2,    AUTO_ACTION_DOWN,       HOOK_NONE},      // 下移选项
    {20,   AUTO_ACTION_NONE,       HOOK_NONE},
    {2,    AUTO_ACTION_UP,         HOOK_NONE},      // 上移选项
    {20,   AUTO_ACTION_NONE,       HOOK_NONE},
    {2,    AUTO_ACTION_RETURN,     HOOK_NONE},      // 退出
    {3000, AUTO_ACTION_NONE,       HOOK_EXIT},      // 等待退出
};

// ========================
// 测试用例注册表
// ========================

static const TestCase test_cases[] = {
    {"Picture",       0,  picture_steps,    9, 20},
    {"2048",          4,  game_2048_steps,  11, 20},
    {"Settings",      3,  settings_steps,    9, 20},
    {"Idea",          4,  NULL,             0, 20},
};

static const int test_case_count = sizeof(test_cases) / sizeof(test_cases[0]);

// 根据 APP 名称查找测试用例
static const TestCase *find_test_case(const char *app_name)
{
    for (int i = 0; i < test_case_count; i++) {
        if (strcmp(test_cases[i].app_name, app_name) == 0) {
            return &test_cases[i];
        }
    }
    return NULL;
}

// ========================
// 钩子检查函数
// ========================

// 检查导航是否完成：cur_app_index 发生变化
static bool hook_nav_check(void)
{
    if (!app_controller) return false;
    int cur_idx = app_controller->cur_app_index;
    if (cur_idx != test_state.prev_app_index) {
        test_state.prev_app_index = cur_idx;
        return true;
    }
    return false;
}

// 检查 APP 是否已进入：app_exit_flag 变为 1
static bool hook_enter_check(void)
{
    if (!app_controller) return false;
    return (app_controller->app_exit_flag == 1);
}

// 检查 APP 是否已退出：app_exit_flag 变为 0
static bool hook_exit_check(void)
{
    if (!app_controller) return false;
    return (app_controller->app_exit_flag == 0);
}

// 检查 JPG 解码是否完成
static bool hook_loading_check(void)
{
    return (hal_native_is_jpg_decode_done() != 0);
}

// 根据钩子类型检查完成条件
static bool check_hook(int hook_type)
{
    switch (hook_type) {
    case HOOK_NAV:     return hook_nav_check();
    case HOOK_ENTER:   return hook_enter_check();
    case HOOK_EXIT:    return hook_exit_check();
    case HOOK_LOADING: return hook_loading_check();
    case HOOK_NONE:
    default:           return false;
    }
}

// ========================
// 启动测试用例
// ========================

static void start_test_case(const TestCase *tc)
{
    test_state.current_case = tc;
    test_state.current_step = 0;
    test_state.frame_counter = 0;
    test_state.nav_counter = 0;
    test_state.case_result = true;
    test_state.nav_first_action_sent = false;
    test_state.enter_go_sent = false;

    // 动态计算导航步数：从当前 app_index 到目标 app_index
    int cur_idx = app_controller ? app_controller->cur_app_index : 0;
    int app_num = app_controller ? app_controller->app_num : 8;
    int target_idx = app_controller ? app_controller->getAppIdxByName(tc->app_name) : tc->nav_steps;

    test_state.target_app_index = target_idx;
    test_state.prev_app_index = cur_idx;

    if (target_idx < 0) {
        LOG_ERROR(AUTO_TEST_TAG, "App '%s' not found in appList!", tc->app_name);
        test_state.dynamic_nav_steps = 0;
    } else {
        int right_steps = (target_idx - cur_idx + app_num) % app_num;
        int left_steps = (cur_idx - target_idx + app_num) % app_num;

        // TURN_RIGHT 使 cur_app_index 减小（列表向左移），TURN_LEFT 使索引增大（列表向右移）
        // 因此：列表向右走 → TURN_LEFT (负值)，列表向左走 → TURN_RIGHT (正值)
        if (right_steps <= left_steps) {
            test_state.dynamic_nav_steps = -right_steps;
        } else {
            test_state.dynamic_nav_steps = left_steps;
        }
    }

    LOG_INFO(AUTO_TEST_TAG, "===== Starting test: %s =====", tc->app_name);
    LOG_DEBUG(AUTO_TEST_TAG, "Current app_index=%d, target app_index=%d, app_num=%d",
              cur_idx, target_idx, app_num);

    if (test_state.dynamic_nav_steps == 0) {
        LOG_DEBUG(AUTO_TEST_TAG, "Phase 0: Already at %s, skipping navigation", tc->app_name);
        test_state.phase = 1;
        test_state.prev_enter_flag = app_controller ? app_controller->app_exit_flag : 0;
    } else {
        LOG_DEBUG(AUTO_TEST_TAG, "Phase 0: Navigating to %s (%d steps, %s)",
                  tc->app_name, abs(test_state.dynamic_nav_steps),
                  test_state.dynamic_nav_steps > 0 ? "right" : "left");
        test_state.phase = 0;
    }
}

// ========================
// 对外接口实现
// ========================

void auto_test_init(int argc, char *argv[])
{
    if (test_state.inited) return;
    test_state.inited = true;

    const char *target = NULL;

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--auto-test=", 12) == 0) {
            target = argv[i] + 12;
            break;
        }
        if (strcmp(argv[i], "--auto-test") == 0 && i + 1 < argc) {
            target = argv[i + 1];
            break;
        }
    }

    if (!target) {
        target = getenv("AUTO_TEST");
    }

    if (target && strlen(target) == 0) {
        target = NULL;
    }

    if (target) {
        test_state.target_app = target;
        LOG_INFO(AUTO_TEST_TAG, "Auto-test mode enabled, target: %s", target);
    }
}

void auto_test_tick(void)
{
    if (!test_state.inited) return;
    if (!test_state.target_app) return;

    if (!test_state.running && !test_state.completed) {
        const TestCase *tc = find_test_case(test_state.target_app);
        if (!tc) {
            LOG_ERROR(AUTO_TEST_TAG, "No test case found for '%s'", test_state.target_app);
            LOG_ERROR(AUTO_TEST_TAG, "Available: Picture, 2048, Settings");
            test_state.running = true;
            return;
        }
        start_test_case(tc);
        test_state.running = true;
        test_state.total_tests = 1;
    }

    if (!test_state.current_case) return;

    const TestCase *tc = test_state.current_case;
    isCheckAction = true;

    switch (test_state.phase) {

    // ============================================================
    // Phase 0: 导航阶段（钩子驱动）
    // 使用 cur_app_index 变化检测导航完成，替代固定 10 帧等待
    // ============================================================
    case 0: {
        int cur_idx = app_controller ? app_controller->cur_app_index : 0;

        if (!test_state.nav_first_action_sent) {
            // 首次进入：记录当前索引，发送第一个导航动作
            test_state.prev_app_index = cur_idx;
            int action = (test_state.dynamic_nav_steps > 0) ? AUTO_ACTION_TURN_RIGHT : AUTO_ACTION_TURN_LEFT;
            hal_native_inject_action(action);
            test_state.nav_counter = 1;
            test_state.nav_first_action_sent = true;
            test_state.frame_counter = 0;
            LOG_DEBUG(AUTO_TEST_TAG, "Nav step 1/%d: sent %s (hook wait)",
                      abs(test_state.dynamic_nav_steps),
                      test_state.dynamic_nav_steps > 0 ? "RIGHT" : "LEFT");
        }

        test_state.frame_counter++;

        // 钩子检测：cur_app_index 是否已变化
        if (cur_idx != test_state.prev_app_index) {
            test_state.prev_app_index = cur_idx;
            test_state.frame_counter = 0;

            if (test_state.nav_counter < abs(test_state.dynamic_nav_steps)) {
                // 还有更多导航步骤
                int action = (test_state.dynamic_nav_steps > 0) ? AUTO_ACTION_TURN_RIGHT : AUTO_ACTION_TURN_LEFT;
                hal_native_inject_action(action);
                test_state.nav_counter++;
                LOG_DEBUG(AUTO_TEST_TAG, "Nav step %d/%d: sent %s (hook triggered, index=%d)",
                          test_state.nav_counter, abs(test_state.dynamic_nav_steps),
                          test_state.dynamic_nav_steps > 0 ? "RIGHT" : "LEFT", cur_idx);
            } else {
                LOG_DEBUG(AUTO_TEST_TAG, "Phase 0 complete (hook: all nav steps done)");
                test_state.phase = 1;
                test_state.frame_counter = 0;
                test_state.prev_enter_flag = app_controller ? app_controller->app_exit_flag : 0;
            }
        }

        // 超时保护：导航步骤超过 500 帧未完成则强制推进
        if (test_state.frame_counter >= 500) {
            LOG_WARN(AUTO_TEST_TAG, "Nav timeout at step %d/%d (waited 500 frames), forcing advance",
                     test_state.nav_counter, abs(test_state.dynamic_nav_steps));
            // 强制执行剩余导航步骤以完成导航
            while (test_state.nav_counter < abs(test_state.dynamic_nav_steps)) {
                int action = (test_state.dynamic_nav_steps > 0) ? AUTO_ACTION_TURN_RIGHT : AUTO_ACTION_TURN_LEFT;
                hal_native_inject_action(action);
                test_state.nav_counter++;
            }
            test_state.phase = 1;
            test_state.frame_counter = 0;
            test_state.prev_enter_flag = app_controller ? app_controller->app_exit_flag : 0;
        }
        break;
    }

    // ============================================================
    // Phase 1: 进入 APP 阶段（钩子驱动）
    // 使用 app_exit_flag 检测进入完成，替代固定 15 帧等待
    // ============================================================
    case 1: {
        if (!test_state.enter_go_sent) {
            // 发送 GO_FORWORD 进入 APP
            test_state.prev_enter_flag = app_controller ? app_controller->app_exit_flag : 0;
            hal_native_inject_action(AUTO_ACTION_GO_FORWORD);
            test_state.enter_go_sent = true;
            test_state.frame_counter = 0;
            LOG_DEBUG(AUTO_TEST_TAG, "Phase 1: GO_FORWORD sent (hook wait for app_exit_flag=1)");
        }

        test_state.frame_counter++;

        // 钩子检测：app_exit_flag 是否变为 1
        if (app_controller && app_controller->app_exit_flag == 1) {
            // 验证是否进入了正确的 APP
            if (app_controller->cur_app_index != test_state.target_app_index) {
                LOG_ERROR(AUTO_TEST_TAG, "Phase 1: Entered wrong APP! expected=%s(idx=%d), actual idx=%d",
                          tc->app_name, test_state.target_app_index, app_controller->cur_app_index);
                test_state.case_result = false;
            }
            LOG_DEBUG(AUTO_TEST_TAG, "Phase 1 complete (hook: app_exit_flag=1, %d frames, idx=%d)",
                      test_state.frame_counter, app_controller->cur_app_index);
            test_state.phase = 2;
            test_state.frame_counter = 0;
            test_state.current_step = 0;
        }

        // 超时保护：2000 帧未进入则强制推进
        if (test_state.frame_counter >= 2000) {
            LOG_WARN(AUTO_TEST_TAG, "Phase 1 timeout (2000 frames), forcing advance");
            test_state.phase = 2;
            test_state.frame_counter = 0;
            test_state.current_step = 0;
        }
        break;
    }

    // ============================================================
    // Phase 2: 测试阶段（钩子 + 帧计数混合驱动）
    // HOOK_NONE: 使用 frame_delay 帧计数
    // HOOK_LOADING/ENTER/EXIT: 等待钩子信号，frame_delay 为超时
    // ============================================================
    case 2: {
        if (test_state.current_step < tc->step_count) {
            const TestStep *step = &tc->steps[test_state.current_step];
            test_state.frame_counter++;

            bool step_complete = false;

            if (step->hook_type == HOOK_NONE) {
                // 无钩子：纯帧计数等待
                step_complete = (test_state.frame_counter >= step->frame_delay);
            } else {
                // 有钩子：等待钩子信号，或超时
                bool hook_triggered = check_hook(step->hook_type);
                bool timeout = (test_state.frame_counter >= step->frame_delay);

                if (hook_triggered) {
                    step_complete = true;
                    LOG_DEBUG(AUTO_TEST_TAG, "Step %d/%d: hook triggered (%d frames)",
                              test_state.current_step + 1, tc->step_count,
                              test_state.frame_counter);
                } else if (timeout) {
                    step_complete = true;
                    LOG_WARN(AUTO_TEST_TAG, "Step %d/%d: hook timeout (%d frames), forcing advance",
                             test_state.current_step + 1, tc->step_count,
                             test_state.frame_counter);
                }
            }

            if (step_complete) {
                test_state.frame_counter = 0;
                if (step->action != AUTO_ACTION_NONE) {
                    hal_native_inject_action(step->action);
                    LOG_DEBUG(AUTO_TEST_TAG, "Step %d/%d: action=%d",
                              test_state.current_step + 1, tc->step_count, step->action);
                }
                test_state.current_step++;
            }
        } else {
            LOG_INFO(AUTO_TEST_TAG, "Phase 2 complete, test PASSED");
            test_state.phase = 3;
            test_state.passed_tests++;
        }
        break;
    }

    // ============================================================
    // Phase 3: 完成阶段
    // ============================================================
    case 3:
        LOG_INFO(AUTO_TEST_TAG, "===== Test '%s' PASSED =====", tc->app_name);
        LOG_INFO(AUTO_TEST_TAG, "Results: %d/%d passed", test_state.passed_tests, test_state.total_tests);
        test_state.running = false;
        test_state.completed = true;
        test_state.phase = 4;
        test_state.frame_counter = 0;
        break;

    // ============================================================
    // Phase 4: 延迟退出阶段
    // ============================================================
    case 4:
        test_state.frame_counter++;
        if (test_state.frame_counter >= 30) {
            LOG_INFO(AUTO_TEST_TAG, "Auto-test complete, exiting...");
            exit(0);
        }
        break;
    }
}

bool auto_test_is_running(void)
{
    return test_state.running && test_state.phase < 3;
}

const char *auto_test_get_target(void)
{
    return test_state.target_app;
}

// ========================
// 弱函数覆盖：hal_native_auto_test_hook
// ========================
extern "C" void hal_native_auto_test_hook(void)
{
    auto_test_tick();
}

// ========================
// 各 APP 测试函数（供外部调用）
// ========================

bool auto_test_app_picture(void)
{
    LOG_DEBUG(AUTO_TEST_TAG, "Running Picture app test...");
    const TestCase *tc = find_test_case("Picture");
    if (!tc) return false;
    start_test_case(tc);
    test_state.running = true;
    return true;
}

bool auto_test_app_game_2048(void)
{
    LOG_DEBUG(AUTO_TEST_TAG, "Running 2048 app test...");
    const TestCase *tc = find_test_case("2048");
    if (!tc) return false;
    start_test_case(tc);
    test_state.running = true;
    return true;
}

bool auto_test_app_settings(void)
{
    LOG_DEBUG(AUTO_TEST_TAG, "Running Settings app test...");
    const TestCase *tc = find_test_case("Settings");
    if (!tc) return false;
    start_test_case(tc);
    test_state.running = true;
    return true;
}

// 以下为预留的 APP 测试函数（尚未实现具体测试步骤）
bool auto_test_app_heartbeat(void)   { LOG_WARN(AUTO_TEST_TAG, "Heartbeat test not implemented"); return false; }
bool auto_test_app_idea_anim(void)   { LOG_WARN(AUTO_TEST_TAG, "IdeaAnim test not implemented"); return false; }
bool auto_test_app_media_player(void){ LOG_WARN(AUTO_TEST_TAG, "MediaPlayer test not implemented"); return false; }
bool auto_test_app_screen_share(void){ LOG_WARN(AUTO_TEST_TAG, "ScreenShare test not implemented"); return false; }
bool auto_test_app_stockmarket(void) { LOG_WARN(AUTO_TEST_TAG, "StockMarket test not implemented"); return false; }
bool auto_test_app_weather(void)     { LOG_WARN(AUTO_TEST_TAG, "Weather test not implemented"); return false; }
bool auto_test_app_tomato(void)      { LOG_WARN(AUTO_TEST_TAG, "Tomato test not implemented"); return false; }
bool auto_test_app_anniversary(void) { LOG_WARN(AUTO_TEST_TAG, "Anniversary test not implemented"); return false; }
bool auto_test_app_game_snake(void)  { LOG_WARN(AUTO_TEST_TAG, "GameSnake test not implemented"); return false; }
bool auto_test_app_bilibili(void)    { LOG_WARN(AUTO_TEST_TAG, "Bilibili test not implemented"); return false; }
bool auto_test_app_pc_resource(void) { LOG_WARN(AUTO_TEST_TAG, "PCResource test not implemented"); return false; }
bool auto_test_app_file_manager(void){ LOG_WARN(AUTO_TEST_TAG, "FileManager test not implemented"); return false; }
bool auto_test_app_LHLXW(void)       { LOG_WARN(AUTO_TEST_TAG, "LHLXW test not implemented"); return false; }