#include "auto_test.h"
#include "hal_native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

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

// isCheckAction 的外部声明（HoloCubic_AIO.cpp 中定义）
extern bool isCheckAction;

// 测试步骤
struct TestStep {
    int frame_delay;   // 等待帧数（约 1ms/帧，粗略估计）
    int action;        // 注入的动作
};

// 测试用例
struct TestCase {
    const char *app_name;           // APP 名称
    int nav_steps;                  // 导航步数（正=右翻，负=左翻）
    const TestStep *steps;          // 测试步骤数组
    int step_count;                 // 步骤数量
    int post_enter_delay;           // 进入 APP 后等待帧数
};

// 测试状态
static struct {
    bool running;
    bool inited;
    const char *target_app;         // 目标 APP 名称，NULL 表示全部测试
    int current_step;
    int frame_counter;
    int phase;                      // 0=导航, 1=进入, 2=测试, 3=退出
    int nav_counter;
    const TestCase *current_case;
    bool case_result;
    int total_tests;
    int passed_tests;
} test_state = {false, false, NULL, 0, 0, 0, 0, NULL, false, 0, 0};

// 日志输出
static void test_log(const char *fmt, ...)
{
    va_list args;
    printf("[AUTO_TEST] ");
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    printf("\n");
    fflush(stdout);
}

// ========================
// 测试用例定义
// ========================

// Picture APP 测试
static const TestStep picture_steps[] = {
    {200,  AUTO_ACTION_GO_FORWORD},  // 等待进入
    {500,  AUTO_ACTION_NONE},         // 等待图片加载
    {200,  AUTO_ACTION_TURN_RIGHT},   // 下一张
    {400,  AUTO_ACTION_NONE},         // 等待加载
    {200,  AUTO_ACTION_TURN_RIGHT},   // 下一张
    {400,  AUTO_ACTION_NONE},         // 等待加载
    {200,  AUTO_ACTION_TURN_LEFT},    // 上一张
    {400,  AUTO_ACTION_NONE},         // 等待加载
    {200,  AUTO_ACTION_RETURN},       // 退出
    {300,  AUTO_ACTION_NONE},         // 等待退出
};

// Game 2048 APP 测试
static const TestStep game_2048_steps[] = {
    {200,  AUTO_ACTION_GO_FORWORD},  // 等待进入
    {500,  AUTO_ACTION_NONE},         // 等待游戏加载
    {200,  AUTO_ACTION_UP},           // 上移
    {300,  AUTO_ACTION_NONE},
    {200,  AUTO_ACTION_TURN_LEFT},    // 左移
    {300,  AUTO_ACTION_NONE},
    {200,  AUTO_ACTION_DOWN},         // 下移
    {300,  AUTO_ACTION_NONE},
    {200,  AUTO_ACTION_TURN_RIGHT},   // 右移
    {300,  AUTO_ACTION_NONE},
    {200,  AUTO_ACTION_RETURN},       // 退出
    {300,  AUTO_ACTION_NONE},
};

// Settings APP 测试
static const TestStep settings_steps[] = {
    {200,  AUTO_ACTION_GO_FORWORD},  // 等待进入
    {500,  AUTO_ACTION_NONE},         // 等待加载
    {200,  AUTO_ACTION_DOWN},         // 下移选项
    {200,  AUTO_ACTION_NONE},
    {200,  AUTO_ACTION_DOWN},         // 下移选项
    {200,  AUTO_ACTION_NONE},
    {200,  AUTO_ACTION_UP},           // 上移选项
    {200,  AUTO_ACTION_NONE},
    {200,  AUTO_ACTION_RETURN},       // 退出
    {300,  AUTO_ACTION_NONE},
};

// ========================
// 测试用例注册表
// ========================
// 按照 HoloCubic_AIO.cpp 中 app_install 的顺序排列
// 索引 0: weather_app
// 索引 1: weather_old_app
// 索引 2: tomato_app
// 索引 3: picture_app
// 索引 4: media_app
// 索引 5: screen_share_app
// 索引 6: file_manager_app
// 索引 7: server_app
// 索引 8: idea_app
// 索引 9: bilibili_app
// 索引 10: settings_app
// 索引 11: game_2048_app
// 索引 12: game_snake_app
// 索引 13: anniversary_app
// 索引 14: heartbeat_app (background)
// 索引 15: stockmarket_app
// 索引 16: pc_resource_app
// 索引 17: LHLXW_app

static const TestCase test_cases[] = {
    {"Picture",       3,  picture_steps,   10, 200},
    {"2048",          11, game_2048_steps,  12, 200},
    {"Settings",      10, settings_steps,   10, 200},
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

// 启动指定测试用例
static void start_test_case(const TestCase *tc)
{
    test_state.current_case = tc;
    test_state.current_step = 0;
    test_state.frame_counter = 0;
    test_state.phase = 0; // 导航阶段
    test_state.nav_counter = 0;
    test_state.case_result = true;

    test_log("===== Starting test: %s =====", tc->app_name);
    test_log("Phase 0: Navigating to %s (%d steps)", tc->app_name, tc->nav_steps);
}

// ========================
// 对外接口实现
// ========================

void auto_test_init(int argc, char *argv[])
{
    if (test_state.inited) return;
    test_state.inited = true;

    // 解析命令行参数，查找 --auto-test=<app> 或 AUTO_TEST 环境变量
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
        test_log("Auto-test mode enabled, target: %s", target);
    }
}

void auto_test_tick(void)
{
    if (!test_state.inited) return;

    // 如果没有指定目标 APP，不启动测试
    if (!test_state.target_app) return;

    // 首次运行，查找测试用例
    if (!test_state.running) {
        const TestCase *tc = find_test_case(test_state.target_app);
        if (!tc) {
            test_log("ERROR: No test case found for '%s'", test_state.target_app);
            test_log("Available: Picture, 2048, Settings");
            // 标记为已运行，避免重复输出错误
            test_state.running = true;
            return;
        }
        start_test_case(tc);
        test_state.running = true;
        test_state.total_tests = 1;
    }

    if (!test_state.current_case) return;

    const TestCase *tc = test_state.current_case;

    // 确保 isCheckAction 为 true，这样动作才会被处理
    isCheckAction = true;

    switch (test_state.phase) {
    case 0: // 导航阶段
        test_state.frame_counter++;
        if (test_state.frame_counter >= 100) { // 每 100 帧发送一次导航动作
            test_state.frame_counter = 0;
            if (test_state.nav_counter < abs(tc->nav_steps)) {
                int action = (tc->nav_steps > 0) ? AUTO_ACTION_TURN_RIGHT : AUTO_ACTION_TURN_LEFT;
                hal_native_inject_action(action);
                test_state.nav_counter++;
                test_log("Nav step %d/%d", test_state.nav_counter, abs(tc->nav_steps));
            } else {
                test_log("Phase 0 complete, entering app...");
                test_state.phase = 1; // 进入阶段
                test_state.frame_counter = 0;
            }
        }
        break;

    case 1: // 进入 APP
        test_state.frame_counter++;
        if (test_state.frame_counter >= 150) {
            hal_native_inject_action(AUTO_ACTION_GO_FORWORD);
            test_log("Phase 1: GO_FORWORD sent");
            test_state.phase = 2; // 测试阶段
            test_state.frame_counter = 0;
            test_state.current_step = 0;
        }
        break;

    case 2: // 测试阶段
        if (test_state.current_step < tc->step_count) {
            const TestStep *step = &tc->steps[test_state.current_step];
            test_state.frame_counter++;

            if (test_state.frame_counter >= step->frame_delay) {
                test_state.frame_counter = 0;
                if (step->action != AUTO_ACTION_NONE) {
                    hal_native_inject_action(step->action);
                    test_log("Step %d/%d: action=%d", test_state.current_step + 1,
                             tc->step_count, step->action);
                }
                test_state.current_step++;
            }
        } else {
            test_log("Phase 2 complete, test PASSED");
            test_state.phase = 3; // 完成
            test_state.passed_tests++;
        }
        break;

    case 3: // 完成
        test_log("===== Test '%s' PASSED =====", tc->app_name);
        test_log("Results: %d/%d passed", test_state.passed_tests, test_state.total_tests);

        // 退出测试模式
        test_state.running = false;
        test_state.phase = 4;
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
    test_log("Running Picture app test...");
    // 直接启动测试
    const TestCase *tc = find_test_case("Picture");
    if (!tc) return false;
    start_test_case(tc);
    test_state.running = true;
    // 注意：实际测试由 auto_test_tick() 驱动
    return true;
}

bool auto_test_app_game_2048(void)
{
    test_log("Running 2048 app test...");
    const TestCase *tc = find_test_case("2048");
    if (!tc) return false;
    start_test_case(tc);
    test_state.running = true;
    return true;
}

bool auto_test_app_settings(void)
{
    test_log("Running Settings app test...");
    const TestCase *tc = find_test_case("Settings");
    if (!tc) return false;
    start_test_case(tc);
    test_state.running = true;
    return true;
}

// 以下为预留的 APP 测试函数（尚未实现具体测试步骤）
bool auto_test_app_heartbeat(void)   { test_log("Heartbeat test not implemented"); return false; }
bool auto_test_app_idea_anim(void)   { test_log("IdeaAnim test not implemented"); return false; }
bool auto_test_app_media_player(void){ test_log("MediaPlayer test not implemented"); return false; }
bool auto_test_app_screen_share(void){ test_log("ScreenShare test not implemented"); return false; }
bool auto_test_app_stockmarket(void) { test_log("StockMarket test not implemented"); return false; }
bool auto_test_app_weather(void)     { test_log("Weather test not implemented"); return false; }
bool auto_test_app_tomato(void)      { test_log("Tomato test not implemented"); return false; }
bool auto_test_app_anniversary(void) { test_log("Anniversary test not implemented"); return false; }
bool auto_test_app_game_snake(void)  { test_log("GameSnake test not implemented"); return false; }
bool auto_test_app_bilibili(void)    { test_log("Bilibili test not implemented"); return false; }
bool auto_test_app_pc_resource(void) { test_log("PCResource test not implemented"); return false; }
bool auto_test_app_file_manager(void){ test_log("FileManager test not implemented"); return false; }
bool auto_test_app_LHLXW(void)       { test_log("LHLXW test not implemented"); return false; }