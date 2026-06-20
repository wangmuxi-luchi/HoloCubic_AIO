# lint.ps1 - 语法检查脚本（使用与 PlatformIO 编译相同的 MinGW g++）
# 用法: .\lint.ps1 <file1> [file2] ...
# 示例: .\lint.ps1 hal/native/auto_test.cpp src/app/idea_anim/idea.cpp

param(
    [Parameter(Mandatory=$true, ValueFromRemainingArguments=$true)]
    [string[]]$Files
)

$ErrorActionPreference = "Stop"

$GPP = "C:\MyPrograms\msys2\mingw64\bin\g++.exe"
if (-not (Test-Path $GPP)) {
    Write-Error "g++ not found: $GPP"
    exit 2
}

$ProjectRoot = $PSScriptRoot

$CommonFlags = @(
    "-std=c++11",
    "-fpermissive",
    "-fcommon",
    "-include", "FreeRTOS.h",
    "-fsyntax-only",
    "-DNATIVE_SIMULATION",
    "-DARDUINO=100",
    "-DLV_CONF_INCLUDE_SIMPLE",
    "-DLV_LVGL_H_INCLUDE_SIMPLE",
    "-DLV_DRV_NO_CONF",
    "-DUSE_SDL",
    "-DSDL_HOR_RES=240",
    "-DSDL_VER_RES=240",
    "-DSDL_ZOOM=2",
    "-DSDL_INCLUDE_PATH=`"SDL2/SDL.h`"",
    "-DSDL_MAIN_HANDLED",
    "-DLV_MEM_CUSTOM=1",
    "-DLV_MEM_SIZE=(128U * 1024U)",
    "-DAPP_EXAMPLE_USE=1",
    "-DAPP_ANNIVERSARY_USE=1",
    "-DAPP_BILIBILI_FANS_USE=1",
    "-DAPP_FILE_MANAGER_USE=0",
    "-DAPP_GAME_2048_USE=1",
    "-DAPP_GAME_SNAKE_USE=1",
    "-DAPP_HEARTBEAT_USE=1",
    "-DAPP_IDEA_ANIM_USE=1",
    "-DAPP_MEDIA_PLAYER_USE=0",
    "-DAPP_PICTURE_USE=1",
    "-DAPP_PC_RESOURCE_USE=1",
    "-DAPP_SCREEN_SHARE_USE=1",
    "-DAPP_SETTING_USE=1",
    "-DAPP_STOCK_MARKET_USE=1",
    "-DAPP_WEATHER_USE=0",
    "-DAPP_WEATHER_OLD_USE=0",
    "-DAPP_TOMATO_USE=1",
    "-DAPP_LHLXW_USE=0",
    "-DAPP_WEB_SERVER_USE=0",
    "-Ihal/native",
    "-Ilib/FreeRTOS-Kernel/include",
    "-Ilib/FreeRTOS-Kernel/portable/MSVC-MingW",
    "-Ilib/lvgl-v8.3",
    "-Ilib/lvgl-v8.3/src",
    "-Ilib/ArduinoJson-6.x/src",
    "-Ilib/font",
    "-Isrc",
    "-Isrc/driver",
    "-Isrc/sys",
    "-Isrc/app"
)

$Total = $Files.Count
$Passed = 0
$Failed = 0

foreach ($f in $Files) {
    $full = if ([System.IO.Path]::IsPathRooted($f)) { $f } else { Join-Path $ProjectRoot $f }
    if (-not (Test-Path $full)) {
        Write-Host "[SKIP]  $f (not found)" -ForegroundColor Yellow
        continue
    }

    Write-Host -NoNewline "[LINT]  $f ... "
    $args = $CommonFlags + @($full)
    $result = & $GPP $args 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "OK" -ForegroundColor Green
        $Passed++
    } else {
        Write-Host "FAIL" -ForegroundColor Red
        Write-Host $result
        $Failed++
    }
}

Write-Host ""
Write-Host "=== $Passed passed, $Failed failed, $Total total ==="
exit $Failed