# HoloCubic_AIO 仿真框架使用教程

> 在 Windows 桌面运行 HoloCubic_AIO 固件，无需真实硬件，支持自动化测试。

---

## 目录

- [1. 环境准备](#1-环境准备)
- [2. 编译](#2-编译)
- [3. 运行](#3-运行)
- [4. 自动测试](#4-自动测试)
- [5. 键盘操作映射](#5-键盘操作映射)
- [6. 架构说明](#6-架构说明)
- [7. 端口说明](#7-端口说明)
- [8. 常见问题](#8-常见问题)

---

## 1. 环境准备

### 1.1 必需软件

| 软件                 | 版本  | 用途              | 安装方式                                                    |
| -------------------- | ----- | ----------------- | ----------------------------------------------------------- |
| **MSYS2**      | 最新  | MinGW64 编译环境  | `winget install msys2` 或 [官网下载](https://www.msys2.org/) |
| **SDL2**       | 2.32+ | 模拟 TFT 屏幕显示 | MSYS2 内安装                                                |
| **PlatformIO** | 最新  | 编译框架          | `pip install platformio` 或 VS Code 插件                  |

### 1.2 安装 SDL2

打开 MSYS2 MINGW64 终端，执行：

```bash
pacman -S mingw-w64-x86_64-SDL2
```

### 1.3 验证环境

```bash
# 检查 gcc 版本（需 16.x）
gcc --version

# 检查 SDL2 是否安装
pkg-config --libs sdl2
# 应输出类似: -lSDL2 -lmingw32 -lSDL2main -lSDL2 -mwindows
```

---

## 2. 编译

### 2.1 编译命令

在项目根目录 `AIO_Firmware_PIO/` 下执行：

```bash
# 方式一：MSYS2 MINGW64 终端（推荐，进入项目目录后直接执行）
# 注意：platformio.exe 路径需替换为你自己的目录
/c/Users/<YourUserName>/.platformio/penv/Scripts/platformio.exe run -e AIO_native_sim

# 方式二：任意终端（通过 MSYS2 shell 包装，自动切换环境）
# 注意：MSYS2 安装路径、platformio.exe 路径和项目路径需替换为你自己的目录
C:\MyPrograms\msys2\msys2_shell.cmd -mingw64 -defterm -no-start -c ^
  "cd /path/to/HoloCubic_AIO/AIO_Firmware_PIO && /c/Users/<YourUserName>/.platformio/penv/Scripts/platformio.exe run -e AIO_native_sim > build_log.txt 2>&1"
```

编译产物：`.pio/build/AIO_native_sim/program.exe`

### 2.2 编译特性

- **增量编译**：仅重新编译修改过的文件，未修改文件复用缓存
- **首次编译耗时**：约 2-3 分钟
- **增量编译耗时**：约 10-15 秒
- **编译输出**：0 error, 0 warning

### 2.3 编译输出保存

如需保存编译日志到文件：

```bash
pio run -e AIO_native_sim > build_log.txt 2>&1
```

---

## 3. 运行

### 3.1 手动运行（无自动测试）

直接双击 `program.exe`，或在终端执行：

```bash
./.pio/build/AIO_native_sim/program.exe
```

弹出一个 240×240 的 SDL 窗口，显示固件 UI，可用键盘交互操作。

### 3.2 带自动测试运行

```bash
# 运行指定 APP 的自动测试
./.pio/build/AIO_native_sim/program.exe --auto-test="WebServer"
./.pio/build/AIO_native_sim/program.exe --auto-test="File Manager"
./.pio/build/AIO_native_sim/program.exe --auto-test="Picture"
./.pio/build/AIO_native_sim/program.exe --auto-test="2048"
./.pio/build/AIO_native_sim/program.exe --auto-test="Settings"

# 通过环境变量也可指定
set AUTO_TEST=WebServer && ./.pio/build/AIO_native_sim/program.exe
```

程序会自动完成测试并退出，不需要手动操作。

---

## 4. 自动测试

### 4.1 测试框架概述

自动测试框架位于 `hal/native/auto_test.cpp`，基于**状态机 + 钩子驱动**设计：

1. **第1步（导航）**：自动发送 LEFT/RIGHT 按键，将当前 APP 切换到目标 APP
2. **第2步（进入）**：发送 GO_FORWORD（SPACE 键），等待 APP 初始化完成
3. **第3步（测试）**：按预定义步骤执行操作，通过钩子函数等待条件满足

### 4.2 钩子类型

| 钩子                  | 含义          | 等待条件                                     |
| --------------------- | ------------- | -------------------------------------------- |
| `HOOK_NAV`          | 导航完成      | `cur_app_index` 发生变化                   |
| `HOOK_ENTER`        | APP 已进入    | `app_exit_flag == 1`                       |
| `HOOK_EXIT`         | APP 已退出    | `app_exit_flag == 0`                       |
| `HOOK_LOADING`      | 图片加载完成  | `hal_native_is_jpg_decode_done()`          |
| `HOOK_SERVER_READY` | 路由注册完成  | `start_web_config()` 调用完毕              |
| `HOOK_HTTP_READY`   | HTTP 服务就绪 | 连接 `localhost:8080` 并收到 HTTP 200 响应 |
| `HOOK_FTP_READY`    | FTP 服务就绪  | 连接 `localhost:21` 并收到 `220` banner  |

### 4.3 操作类型

| 操作           | 对应键盘  | 说明                     |
| -------------- | --------- | ------------------------ |
| `TURN_LEFT`  | ←        | 向左切换 APP             |
| `TURN_RIGHT` | →        | 向右切换 APP             |
| `GO_FORWORD` | Space     | 进入 APP                 |
| `RETURN`     | Backspace | 退出 APP                 |
| `UP`         | ↑        | 上移                     |
| `DOWN`       | ↓        | 下移                     |
| `NONE`       | —        | 无操作，仅等待帧数或钩子 |

### 4.4 已完成的测试用例

#### 4.4.1 Picture（图片查看器）— 9 步骤

```
导航(6步 LEFT) → 进入(GO_FORWORD) → 等待加载 → 下一张 → 等待加载
→ 下一张 → 等待加载 → 上一张 → 等待加载 → 退出 → 等待退出
```

**验证内容**：JPG 解码、图片切换、APP 进出

#### 4.4.2 2048（2048 游戏）— 11 步骤

```
导航(4步 LEFT) → 进入(GO_FORWORD) → 等待加载 → 上移 → 等待
→ 左移 → 等待 → 下移 → 等待 → 右移 → 等待 → 退出 → 等待退出
```

**验证内容**：复杂 UI 交互、四方向操作、游戏逻辑运行

#### 4.4.3 Settings（设置）— 9 步骤

```
导航(3步 LEFT) → 进入(GO_FORWORD) → 等待加载 → 下移 → 等待
→ 下移 → 等待 → 上移 → 等待 → 退出 → 等待退出
```

**验证内容**：菜单导航、选项切换

#### 4.4.4 WebServer（HTTP 服务器）— 5 步骤

```
导航(6步 LEFT) → 进入(GO_FORWORD) → 等待路由注册 → 等待 HTTP 就绪
→ 退出 → 等待退出
```

**验证内容**：

- `HOOK_SERVER_READY`：等待 `start_web_config()` 完成 28 条路由注册
- `HOOK_HTTP_READY`：通过 Winsock 连接 `localhost:8080`，发送 `GET / HTTP/1.0`，验证返回 `HTTP/1.1 200 OK` + HTML 内容
- 28 条路由：`/`、`/sys_setting`、`/memory`、`/wifi` 等

#### 4.4.5 File Manager（FTP 服务器）— 4 步骤

```
导航(5步 LEFT) → 进入(GO_FORWORD) → 等待 FTP 就绪 → 退出 → 等待退出
```

**验证内容**：

- `HOOK_FTP_READY`：通过 Winsock 连接 `localhost:21`，验证收到 `220 HoloCubic FTP Server Ready` banner
- 认证：用户名 `esp32`，密码 `esp32`
- 支持命令：USER、PASS、PWD、CWD、LIST、RETR、STOR、DELE 等

#### 4.4.6 Idea（创意动画）— 0 步骤

```
导航(4步 LEFT) → 进入(GO_FORWORD)
```

**验证内容**：仅验证 APP 可正常进入，不执行额外操作（0 步骤）

### 4.5 测试结果解读

```
[INFO]  [AUTO_TEST] ===== Starting test: WebServer =====
[INFO]  [AUTO_TEST] ===== Test 'WebServer' PASSED =====
[INFO]  [AUTO_TEST] Results: 1/1 passed
```

- `PASSED`：所有步骤执行完毕，钩子条件全部满足
- `FAILED`：超时（步骤帧数耗尽）或导航失败
- 测试完成后程序自动退出，退出码 0 表示成功

### 4.6 批量运行所有测试

**Bash (Linux/macOS/Git Bash):**
```bash
for test in "Picture" "2048" "Settings" "Idea" "WebServer" "File Manager" "LH&LXW"; do
    echo "=== Testing: $test ==="
    ./.pio/build/AIO_native_sim/program.exe --auto-test="$test"
    echo "Exit code: $?"
done
```

**PowerShell (Windows):**
```powershell
foreach ($test in @("Picture","2048","Settings","Idea","WebServer","File Manager","LH&LXW")) {
    Write-Host "=== Testing: $test ==="
    .\.pio\build\AIO_native_sim\program.exe --auto-test="$test"
    Write-Host "Exit code: $LASTEXITCODE`n"
}
```

---

## 5. 键盘操作映射

| 键盘按键        | 模拟操作 | 说明                |
| --------------- | -------- | ------------------- |
| `←` / `→` | 左右倾斜 | 切换 APP            |
| `↑` / `↓` | 上下倾斜 | 垂直方向操作        |
| `Space`       | 按下确认 | 进入 APP / 确认选项 |
| `Backspace`   | 返回     | 退出 APP / 返回上级 |
| `Enter`       | 摇动     | 模拟摇晃设备        |

---

## 6. 架构说明

### 6.1 文件结构

```
AIO_Firmware_PIO/
├── hal/native/              # HAL 仿真层（核心）
│   ├── auto_test.cpp        # 自动测试框架
│   ├── auto_test.h          # 测试框架头文件
│   ├── hal_native.h         # HAL 层公共接口
│   ├── sim_main.cpp         # 仿真入口（FreeRTOS 调度器）
│   ├── Arduino.h            # Arduino API 桩
│   ├── sdl_display.cpp      # SDL2 显示驱动
│   ├── sdl_input.cpp        # 键盘输入映射
│   ├── flash_fs.cpp         # SPIFFS 文件系统模拟
│   ├── NativeServer.h/cpp   # TCP 服务器基类（Winsock2）
│   ├── WebServer.h/cpp      # HTTP 服务器实现
│   ├── ESP32FtpServer.h/cpp # FTP 服务器实现
│   ├── driver_stubs.cpp     # 硬件驱动桩函数集合
│   └── ...                  # 40+ 其他桩文件
├── src/                     # 固件源码（与 ESP32 共享）
│   ├── app/                 # 17 个 APP 应用
│   ├── driver/              # 硬件驱动（仿真排除）
│   └── sys/                 # 系统模块
├── lib/                     # 第三方库
│   ├── FreeRTOS-Kernel/     # FreeRTOS 真内核
│   ├── lvgl-v8.3/           # LVGL 图形库
│   └── ArduinoJson-6.x/     # JSON 解析
├── platformio.ini           # 编译配置
└── docs/                    # 文档
    └── native-sim-tutorial.md  # 本文档
```

### 6.2 核心原理

```
┌──────────────────────────────────────────────┐
│  AIO 固件源码 (src/)                          │
│  ├── 17 个 APP 应用层（与 ESP32 共用代码）    │
│  ├── 系统模块 (sys/)                          │
│  └── 驱动层 (driver/) — 仿真中排除            │
├──────────────────────────────────────────────┤
│  HAL 仿真层 (hal/native/)                     │
│  ├── 显示: SDL2 240×240 窗口 + LVGL 渲染     │
│  ├── 输入: 键盘映射 → 模拟 MPU6050 手势       │
│  ├── 存储: SPIFFS 内存模拟 (FlashFS)          │
│  ├── 网络: Winsock2 TCP 服务器 (HTTP/FTP)     │
│  └── 驱动桩: 40+ 个空壳函数                   │
├──────────────────────────────────────────────┤
│  FreeRTOS-Kernel (真内核，非模拟)              │
│  ├── 任务调度: xTaskCreate / vTaskDelay       │
│  ├── 队列: xQueueSend / xQueueReceive         │
│  └── 定时器: xTimerCreate                     │
├──────────────────────────────────────────────┤
│  Windows + MinGW64 + SDL2                     │
└──────────────────────────────────────────────┘
```

### 6.3 与真实硬件的对应关系

| 真实硬件          | 仿真实现                        |
| ----------------- | ------------------------------- |
| ESP32 双核        | 单线程模拟（FreeRTOS 协程调度） |
| TFT 屏幕 240×240 | SDL2 窗口 240×240              |
| MPU6050 陀螺仪    | 键盘方向键映射                  |
| SD 卡             | FlashFS 内存文件系统            |
| WiFi STA/AP       | Winsock2 TCP/UDP                |
| SPIFFS            | 内存模拟文件系统                |
| RGB LED           | 桩函数（无操作）                |
| BH1750 光照传感器 | 桩函数（返回固定值）            |

---

## 7. 端口说明

仿真环境使用以下端口（均为 `localhost` 回环地址）：

| 服务           | 端口 | 说明                                                                       |
| -------------- | :--: | -------------------------------------------------------------------------- |
| **HTTP** | 8080 | WebServer 配置页面（原始固件用 80，仿真改用 8080 避免与 Windows 服务冲突） |
| **FTP**  |  21  | File Manager 文件传输（与控制通道）                                        |
| **DNS**  |  53  | 桩函数，不实际监听                                                         |

### 端口冲突解决

如果 8080 也被占用，修改以下两处：

1. `src/app/server/server.cpp`：`#define SERVER_PORT 8080` → 改为其他端口
2. `hal/native/auto_test.cpp`：`htons(8080)` → 同步修改

---

## 8. 常见问题

### Q1: 编译报错 `SDL2/SDL.h: No such file or directory`

SDl2 未安装或未正确配置。确认 MSYS2 中已安装：

```bash
pacman -S mingw-w64-x86_64-SDL2
```

### Q2: 运行时窗口一闪而过

检查 `platformio.ini` 中是否定义了 `-D SDL_MAIN_HANDLED`。该宏必须定义，否则 SDL 会接管 main 函数。

### Q3: 自动测试报 `App 'XXX' not found in appList`

APP 名称不匹配。检查 `auto_test.cpp` 中 `test_cases` 数组的名称是否与 `app_controller.cpp` 中注册的 APP 名称一致（区分大小写）。

### Q4: HTTP 测试返回 404

可能原因：

- 端口被占用：`netstat -ano | findstr ":8080 "` 检查
- 路由未注册：确认 `HOOK_SERVER_READY` 在测试步骤中排在 `HOOK_HTTP_READY` 之前
- 服务器未启动：检查 `APP_MESSAGE_WIFI_AP` 事件是否触发

### Q5: 编译很慢，每次都完全重新编译

PlatformIO 默认支持增量编译。如果每次都完全重编，检查：

- 是否删除了 `.pio` 目录
- `platformio.ini` 中 `lib_archive = false` 是正常的，不影响增量编译
- 确认 `pio run` 输出中 `Compiling .pio\build\AIO_native_sim\...` 的文件数量（增量编译时只有少量文件）

### Q6: 如何添加新的测试用例

1. 在 `auto_test.cpp` 中定义 `TestStep` 数组
2. 在 `test_cases[]` 中注册新条目
3. 格式：`{"APP名称", 导航偏移量, steps数组, 步骤数, 每步骤超时帧数}`

### Q7: 如何调试 APP 崩溃

1. 手动运行 `program.exe`（不带 `--auto-test`），观察控制台输出
2. 检查 `[SETUP]` 和 `[APPCTRL]` 日志定位问题位置
3. 使用 `gdb` 调试：`gdb ./.pio/build/AIO_native_sim/program.exe`

---

## 附录：APP 状态总览

| APP          | 编译 | 功能 | 自动测试 | 备注                  |
| ------------ | :--: | :--: | :------: | --------------------- |
| Weather      |  ✅  |  ✅  |    —    | 天气应用              |
| Weather Old  |  ✅  |  ✅  |    —    | 旧版天气              |
| Tomato       |  ✅  |  ✅  |    —    | 番茄钟                |
| Picture      |  ✅  |  ✅  |    ✅    | 图片查看（9 步骤）    |
| 2048         |  ✅  |  ✅  |    ✅    | 2048 游戏（11 步骤）  |
| Settings     |  ✅  |  ✅  |    ✅    | 系统设置（9 步骤）    |
| WebServer    |  ✅  |  ✅  |    ✅    | HTTP 8080（5 步骤）   |
| File Manager |  ✅  |  ✅  |    ✅    | FTP 21（4 步骤）      |
| IdeaAnim     |  ✅  |  ✅  |    ✅    | 创意动画（0 步骤）    |
| Anniversary  |  ✅  |  ✅  |    —    | 纪念日                |
| Bilibili     |  ✅  |  ✅  |    —    | B站粉丝数             |
| Heartbeat    |  ✅  | ⚠️ |    —    | 网络阻塞，待修复      |
| Snake        |  ✅  | ⚠️ |    —    | 卡顿                  |
| Stockmarket  |  ✅  | ⚠️ |    —    | 卡顿                  |
| Screen Share |  ✅  |  —  |    —    | 仅占位                |
| PC Resource  |  ✅  |  —  |    —    | 编译通过，功能未调试  |
| LHLXW        |  ✅  |  —  |    —    | 编译通过，未测试      |
| Media Player |  ❌  |  ❌  |    —    | 排除（需 DMA/解码器） |
| Example      |  ❌  |  ❌  |    —    | 排除                  |