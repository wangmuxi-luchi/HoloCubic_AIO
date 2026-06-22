#include "weather.h"
#include "weather_gui.h"
#include "ESP32Time.h"
#include "sys/app_controller.h"
#include "network.h"
#include "common.h"
#include "ArduinoJson.h"
#include <esp32-hal-timer.h>
#include <map>
#include "network_async.h"

#define WEATHER_APP_NAME "Weather"
#define WEATHER_NOW_API "https://www.yiketianqi.com/free/day?appid=%s&appsecret=%s&unescape=1&city=%s"
// v1.yiketianqi.com/api?unescape=1&version=v61
// 更新使用高德地图 接口文档 https://developer.amap.com/api/webservice/guide/api/weatherinfo
// 创建服务 https://lbs.amap.com/api/webservice/create-project-and-key
// 天气对照表 https://lbs.amap.com/api/webservice/guide/tools/weather-code/

#define WEATHER_LIVES_API "http://restapi.amap.com/v3/weather/weatherInfo?key=%s&city=%s&extensions=base"
#define WEATHER_DALIY_FORECAST_API "http://restapi.amap.com/v3/weather/weatherInfo?key=%s&city=%s&extensions=all"
#define TIME_API "https://acs.m.taobao.com/gw/mtop.common.getTimestamp/"
#define WEATHER_PAGE_SIZE 2
#define UPDATE_WEATHER 0x01       // 更新天气
#define UPDATE_DALIY_WEATHER 0x02 // 更新每天天气
#define UPDATE_TIME 0x04          // 更新时间

// // NTP 服务器信息
// const char* ntpServer = "ntp.aliyun.com"; // 阿里云NTP服务器
// const long  gmtOffset_sec = 8 * 3600;     // 中国时区（UTC+8）
// const int   daylightOffset_sec = 0;       // 无需夏令时偏移
// // 使用 UDP 连接
// WiFiUDP ntpUDP;
// // 创建NTP客户端
// NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, 60000); // 60秒同步一次
// #define USE_NTP //开启NTP时间获取
// bool isUdpInit = false;

// 天气的持久化配置
#define WEATHER_CONFIG_PATH "/weather_2111.cfg"
struct WT_Config
{
    String tianqi_url;                   // tianqiapi 的url
    String tianqi_city_code;             // 城市名或代码
    String tianqi_api_key;               // api的key
    unsigned long weatherUpdataInterval; // 天气更新的时间间隔(s)
    unsigned long timeUpdataInterval;    // 日期时钟更新的时间间隔(s)
};

static void write_config(WT_Config *cfg)
{
    char tmp[16];
    // 将配置数据保存在文件中（持久化）
    String w_data;
    w_data = w_data + cfg->tianqi_url + "\n";
    w_data = w_data + cfg->tianqi_city_code + "\n";
    w_data = w_data + cfg->tianqi_api_key + "\n";
    memset(tmp, 0, 16);
    snprintf(tmp, 16, "%lu\n", cfg->weatherUpdataInterval);
    w_data += tmp;
    memset(tmp, 0, 16);
    snprintf(tmp, 16, "%lu\n", cfg->timeUpdataInterval);
    w_data += tmp;
    g_flashCfg.writeFile(WEATHER_CONFIG_PATH, w_data.c_str());
}

static void read_config(WT_Config *cfg)
{
    // 如果有需要持久化配置文件 可以调用此函数将数据存在flash中
    // 配置文件名最好以APP名为开头 以".cfg"结尾，以免多个APP读取混乱
    char info[128] = {0};
    uint16_t size = g_flashCfg.readFile(WEATHER_CONFIG_PATH, (uint8_t *)info);
    info[size] = 0;
    if (size == 0)
    {
        // 默认值
        cfg->tianqi_url = "restapi.amap.com/v3/weather/weatherInfo";
        cfg->tianqi_city_code = "北京"; // "110000";
        cfg->tianqi_api_key = "";
        cfg->weatherUpdataInterval = 900000; // 天气更新的时间间隔900000(900s)
        cfg->timeUpdataInterval = 900000;    // 日期时钟更新的时间间隔900000(900s)
        write_config(cfg);
    }
    else
    {
        // 解析数据
        char *param[5] = {0};
        analyseParam(info, 5, param);
        cfg->tianqi_url = param[0];
        cfg->tianqi_city_code = param[1];
        cfg->tianqi_api_key = param[2];
        cfg->weatherUpdataInterval = atol(param[3]);
        cfg->timeUpdataInterval = atol(param[4]);
    }
}

struct WeatherAppRunData
{
    unsigned long preWeatherMillis; // 上一回更新天气时的毫秒数
    unsigned long preTimeMillis;    // 更新时间计数器
    long long preNetTimestamp;      // 上一次的网络时间戳
    long long errorNetTimestamp;    // 网络到显示过程中的时间误差
    long long preLocalTimestamp;    // 上一次的本地机器时间戳
    unsigned int coactusUpdateFlag; // 强制更新标志
    int clock_page;
    unsigned int update_type; // 更新类型的标志位

    BaseType_t xReturned_task_update; // 更新数据的异步任务
    TaskHandle_t xHandle_task_update; // 更新数据的异步任务

    ESP32Time g_rtc; // 用于时间解码
    Weather wea;     // 保存天气状况
};

static WT_Config cfg_data;
static WeatherAppRunData *run_data = NULL;

static HttpRequest *g_weather_now_req = NULL;
static HttpRequest *g_weather_ntp_req = NULL;
static HttpRequest *g_weather_daily_req = NULL;

static bool weather_ui_initialized = false;
static int last_clock_page = -1;
static bool weather_data_changed = false;
static unsigned long last_space_anim_ms = 0;

enum WEA_EVENT_ID
{
    UPDATE_NOW,
    UPDATE_NTP,
    UPDATE_DAILY
};

/*
高德天气接口提供的天气
https://lbs.amap.com/api/webservice/guide/tools/weather-code
*/
// std::map<String, int> weatherMap = {{"qin", 0}, {"yin", 1}, {"yu", 2}, {"yun", 3}, {"bingbao", 4}, {"wu", 5}, {"shachen", 6}, {"lei", 7}, {"xue", 8}};
std::map<String, int> weatherMap = {
    {"晴", 0},
    {"少云", 0},
    {"晴间多云", 3},
    {"多云", 3},
    {"阴", 1},
    {"有风", 3},
    {"平静", 3},
    {"微风", 3},
    {"和风", 3},
    {"清风", 3},
    {"强风/劲风", 3},
    {"疾风", 1},
    {"大风", 1},
    {"烈风", 1},
    {"风暴", 1},
    {"狂爆风", 1},
    {"飓风", 1},
    {"热带风暴", 1},
    {"霾", 5},
    {"中度霾", 5},
    {"重度霾", 5},
    {"严重霾", 5},
    {"阵雨", 2},
    {"雷阵雨", 7},
    {"雷阵雨并伴有冰雹", 4},
    {"小雨", 2},
    {"中雨", 2},
    {"大雨", 2},
    {"暴雨", 2},
    {"大暴雨", 2},
    {"特大暴雨", 2},
    {"强阵雨", 2},
    {"强雷阵雨", 7},
    {"极端降雨", 2},
    {"毛毛雨/细雨", 2},
    {"雨", 2},
    {"小雨-中雨", 2},
    {"中雨-大雨", 2},
    {"大雨-暴雨", 2},
    {"暴雨-大暴雨", 2},
    {"大暴雨-特大暴雨", 2},
    {"雨雪天气", 8},
    {"雨夹雪", 8},
    {"阵雨夹雪", 8},
    {"冻雨", 4},
    {"雪", 8},
    {"阵雪", 8},
    {"小雪", 8},
    {"中雪", 8},
    {"大雪", 8},
    {"暴雪", 8},
    {"小雪-中雪", 8},
    {"中雪-大雪", 8},
    {"大雪-暴雪", 8},
    {"浮尘", 6},
    {"扬沙", 6},
    {"沙尘暴", 6},
    {"强沙尘暴", 6},
    {"龙卷风", 6},
    {"雾", 5},
    {"浓雾", 5},
    {"强浓雾", 5},
    {"轻雾", 5},
    {"大雾", 5},
    {"特强浓雾", 5},
    {"热", 0},
    {"冷", 0},
    {"未知", 0}

};

static void task_update(void *parameter); // 异步更新任务

static int windLevelAnalyse(String str)
{
    int ret = 0;
    for (char ch : str)
    {
        if (ch >= '0' && ch <= '9')
        {
            ret = ret * 10 + (ch - '0');
        }
    }
    return ret;
}

static void get_weather(void)
{
    if (WL_CONNECTED != WiFi.status())
        return;

    HTTPClient http;
    http.setTimeout(1000);
    char api[128] = {0};

    snprintf(api, 128, WEATHER_LIVES_API,
             cfg_data.tianqi_api_key.c_str(),
             cfg_data.tianqi_city_code.c_str());
    Serial.print("API = ");
    Serial.println(api);
    http.begin(api);

    int httpCode = http.GET();
    if (httpCode > 0)
    {
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        {
            String payload = http.getString();
            DynamicJsonDocument doc(768);
            deserializeJson(doc, payload);
            Serial.println(payload);
            if (doc.containsKey("lives"))
            {
                /*
                {
                "status": "1",
                "count": "1",
                "info": "OK",
                "infocode": "10000",
                "lives": [
                    {
                        "province": "河南",
                        "city": "涧西区",
                        "adcode": "410305",
                        "weather": "晴",
                        "temperature": "19",
                        "winddirection": "东北",
                        "windpower": "≤3",
                        "humidity": "38",
                        "reporttime": "2024-03-21 18:30:05",
                        "temperature_float": "19.0",
                        "humidity_float": "38.0"
                    }
                ]
            }
                */
                JsonObject weather_live = doc["lives"][0];
                // 获取城市区域中文
                strcpy(run_data->wea.cityname, weather_live["city"].as<String>().c_str());
                // 温度
                run_data->wea.temperature = weather_live["temperature"].as<int>();
                // 湿度
                run_data->wea.humidity = weather_live["humidity"].as<int>();
                // 天气情况
                run_data->wea.weather_code = weatherMap[weather_live["weather"].as<String>()];
                
                strcpy(run_data->wea.weather, weather_live["weather"].as<String>().c_str());
                // 风速
                strcpy(run_data->wea.windDir, weather_live["winddirection"].as<String>().c_str());
                strcpy(run_data->wea.windpower, weather_live["windpower"].as<String>().c_str());
                Serial.printf("wea.windpower  = %s", run_data->wea.windpower);

                // 空气质量没有这个参数，只能用风速来粗略替换了
                run_data->wea.airQulity = airQulityLevel(run_data->wea.windpower);

                // weather_info.city = weather_live["city"].as<String>();
                // weather_info.weather = weather_live["weather"].as<String>();
                // weather_info.winddirection = weather_live["winddirection"].as<String>();
                // weather_info.windpower = weather_live["windpower"].as<String>();
                // weather_info.temperature = weather_live["temperature"].as<String>();
                // weather_info.humidity = weather_live["humidity"].as<String>();
                // weather_info.animIndex = get_weather_anim(weather_info.weather, getDateTime().hour());
                // weather_info.lastUpdate = millis();

                Serial.println(" Get weather info OK\n");
            }
            else
            {
                // 返回值错误，记录
                Serial.println("[APP] Get weather error,info");
                String err_info = doc["info"];
                Serial.print(err_info);
                Serial.println("");
            }
        }
    }
    else
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

static long long get_timestamp(void)
{
    // 使用本地的机器时钟
    run_data->preNetTimestamp = run_data->preNetTimestamp + (GET_SYS_MILLIS() - run_data->preLocalTimestamp);
    run_data->preLocalTimestamp = GET_SYS_MILLIS();
    Serial.printf("[WEATHER] get_timestamp(local) preNetTimestamp=%lld\r\n", run_data->preNetTimestamp);
    return run_data->preNetTimestamp;
}

static long long get_timestamp(String url)
{
    if (WL_CONNECTED != WiFi.status())
        return 0;

    String time = "";
    HTTPClient http;
    http.setTimeout(1000);
    http.begin(url);

    int httpCode = http.GET();
    if (httpCode > 0)
    {
        if (httpCode == HTTP_CODE_OK)
        {
            String payload = http.getString();
            Serial.println(payload);
            int time_index = payload.indexOf("\"t\":\"") + 5;       // 找到 "t":" 后的索引，+5 跳过 "t":" 的长度
            int time_end_index = payload.indexOf("\"", time_index); // 查找结束引号的位置
            time = payload.substring(time_index, time_end_index);   // 提取时间戳

            // 以网络时间戳为准
            run_data->preNetTimestamp = atoll(time.c_str()) + run_data->errorNetTimestamp + TIMEZERO_OFFSIZE;
            run_data->preLocalTimestamp = GET_SYS_MILLIS();
        }
    }
    else
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
        // 得不到网络时间戳时
        run_data->preNetTimestamp = run_data->preNetTimestamp + (GET_SYS_MILLIS() - run_data->preLocalTimestamp);
        run_data->preLocalTimestamp = GET_SYS_MILLIS();
    }
    http.end();

    return run_data->preNetTimestamp;
}

static void get_daliyWeather(short maxT[], short minT[])
{
    if (WL_CONNECTED != WiFi.status())
        return;

    HTTPClient http;
    http.setTimeout(1000);
    char api[128] = {0};
    snprintf(api, 128, WEATHER_DALIY_FORECAST_API,
             cfg_data.tianqi_api_key.c_str(),
             cfg_data.tianqi_city_code.c_str());
    Serial.print("API = ");
    Serial.println(api);
    http.begin(api);

    int httpCode = http.GET();
    if (httpCode > 0)
    {
        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
        {
            String payload = http.getString();
            Serial.println(payload);
            DynamicJsonDocument doc2(4096);
            deserializeJson(doc2, payload);
            // JsonObject sk = doc2.as<JsonObject>();
            // for (int gDW_i = 0; gDW_i < FORECAST_DAYS; ++gDW_i)
            // {
            //     maxT[gDW_i] = sk["data"][gDW_i]["tem_day"].as<int>();
            //     minT[gDW_i] = sk["data"][gDW_i]["tem_night"].as<int>();
            // }

            if (doc2.containsKey("forecasts"))
            {
                JsonObject weather_forecast = doc2["forecasts"][0];
                for (int i = 0; i < FORECAST_DAYS; i++)
                {
                    maxT[i] = weather_forecast["casts"][i]["daytemp"].as<int>();
                    minT[i] = weather_forecast["casts"][i]["nighttemp"].as<int>();
                }
                Serial.println("Get weather cast OK\n");
            }
        }
    }
    else
    {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
}

static void updateTime_RTC(long long timestamp)
{
    struct TimeStr t;
    run_data->g_rtc.setTime(timestamp / 1000);
    t.month = run_data->g_rtc.getMonth() + 1;
    t.day = run_data->g_rtc.getDay();
    t.hour = run_data->g_rtc.getHour(true);
    t.minute = run_data->g_rtc.getMinute();
    t.second = run_data->g_rtc.getSecond();
    t.weekday = run_data->g_rtc.getDayofWeek();
    Serial.printf("[WEATHER] updateTime_RTC timestamp=%lld -> %d-%02d-%02d %02d:%02d:%02d\r\n",
                  timestamp, t.month, t.day, t.hour, t.minute, t.second);
    display_time(t, LV_SCR_LOAD_ANIM_NONE);
}

static int weather_init(AppController *sys)
{
    tft->setSwapBytes(true);
    weather_gui_init();
    // 获取配置信息
    read_config(&cfg_data);

    // 初始化运行时参数
    run_data = (WeatherAppRunData *)calloc(1, sizeof(WeatherAppRunData));
    memset((char *)&run_data->wea, 0, sizeof(Weather));
    run_data->preNetTimestamp = 1577808000000; // 上一次的网络时间戳 初始化为2020-01-01 00:00:00
    run_data->errorNetTimestamp = 2;
    run_data->preLocalTimestamp = GET_SYS_MILLIS(); // 上一次的本地机器时间戳
    run_data->clock_page = 0;
    run_data->preWeatherMillis = 0;
    run_data->preTimeMillis = 0;
    // 强制更新
    run_data->coactusUpdateFlag = 0x01;
    run_data->update_type = 0x00; // 表示什么也不需要更新

    // 目前更新数据的任务栈大小5000够用，4000不够用
    // 为了后期迭代新功能 当前设置为8000, 任务可能会导致卡死
    run_data->xReturned_task_update = pdFAIL;
    // run_data->xReturned_task_update = xTaskCreate(
    //     task_update,                     /*任务函数*/
    //     "Task_update",                   /*带任务名称的字符串*/
    //     8000,                            /*堆栈大小，单位为字节*/
    //     NULL,                            /*作为任务输入传递的参数*/
    //     1,                               /*任务的优先级*/
    //     &run_data->xHandle_task_update); /*任务句柄*/

    APP_OBJ *app = sys->getAppByName(WEATHER_APP_NAME);
    if (app) {
        app->loop_interval_ms = 100;
    }
    weather_ui_initialized = false;
    last_clock_page = -1;
    weather_data_changed = false;
    last_space_anim_ms = 0;
    return 0;
}

static void weather_process(AppController *sys,
                            const ImuAction *act_info)
{
    lv_scr_load_anim_t anim_type = LV_SCR_LOAD_ANIM_NONE;

    if (RETURN == act_info->active)
    {
        sys->app_exit();
        return;
    }

    if (g_weather_now_req && g_weather_now_req->done)
    {
        __sync_synchronize();
        String payload = String(g_weather_now_req->response);
        if (g_weather_now_req->http_code > 0 && payload.length() > 0)
        {
            DynamicJsonDocument doc(768);
            deserializeJson(doc, payload);
            Serial.println(payload);
            if (doc.containsKey("lives"))
            {
                JsonObject weather_live = doc["lives"][0];
                strcpy(run_data->wea.cityname, weather_live["city"].as<String>().c_str());
                run_data->wea.temperature = weather_live["temperature"].as<int>();
                run_data->wea.humidity = weather_live["humidity"].as<int>();
                run_data->wea.weather_code = weatherMap[weather_live["weather"].as<String>()];
                strcpy(run_data->wea.weather, weather_live["weather"].as<String>().c_str());
                strcpy(run_data->wea.windDir, weather_live["winddirection"].as<String>().c_str());
                strcpy(run_data->wea.windpower, weather_live["windpower"].as<String>().c_str());
                run_data->wea.airQulity = airQulityLevel(run_data->wea.windpower);
                Serial.println(" Get weather info OK\n");
                weather_data_changed = true;
            }
        }
        else
        {
            Serial.printf("[HTTP] GET weather failed\n");
        }
        vPortFree(g_weather_now_req);
        g_weather_now_req = NULL;
    }

    if (g_weather_ntp_req && g_weather_ntp_req->done)
    {
        Serial.printf("[WEATHER] g_weather_ntp_req->done, http_code=%d\r\n", g_weather_ntp_req->http_code);
        __sync_synchronize();
        String payload = String(g_weather_ntp_req->response);
        if (g_weather_ntp_req->http_code > 0 && payload.length() > 0)
        {
            Serial.println(payload);
            int time_index = payload.indexOf("\"t\":\"") + 5;
            int time_end_index = payload.indexOf("\"", time_index);
            String time = payload.substring(time_index, time_end_index);
            run_data->preNetTimestamp = atoll(time.c_str()) + run_data->errorNetTimestamp + TIMEZERO_OFFSIZE;
            run_data->preLocalTimestamp = GET_SYS_MILLIS();
            Serial.printf("[WEATHER] NTP response: raw=%s, preNetTimestamp=%lld, preLocalTimestamp=%lu\r\n",
                          time.c_str(), run_data->preNetTimestamp, run_data->preLocalTimestamp);
            weather_data_changed = true;
        }
        else
        {
            Serial.printf("[HTTP] GET time failed\n");
            run_data->preNetTimestamp = run_data->preNetTimestamp + (GET_SYS_MILLIS() - run_data->preLocalTimestamp);
            run_data->preLocalTimestamp = GET_SYS_MILLIS();
        }
        vPortFree(g_weather_ntp_req);
        g_weather_ntp_req = NULL;
    }

    if (g_weather_daily_req && g_weather_daily_req->done)
    {
        __sync_synchronize();
        String payload = String(g_weather_daily_req->response);
        if (g_weather_daily_req->http_code > 0 && payload.length() > 0)
        {
            Serial.println(payload);
            DynamicJsonDocument doc2(4096);
            deserializeJson(doc2, payload);
            if (doc2.containsKey("forecasts"))
            {
                JsonObject weather_forecast = doc2["forecasts"][0];
                for (int i = 0; i < FORECAST_DAYS; i++)
                {
                    run_data->wea.daily_max[i] = weather_forecast["casts"][i]["daytemp"].as<int>();
                    run_data->wea.daily_min[i] = weather_forecast["casts"][i]["nighttemp"].as<int>();
                }
                Serial.println("Get weather cast OK\n");
            }
        }
        else
        {
            Serial.printf("[HTTP] GET daily weather failed\n");
        }
        vPortFree(g_weather_daily_req);
        g_weather_daily_req = NULL;
    }

    if (GO_FORWORD == act_info->active)
    {
        // 间接强制更新
        run_data->coactusUpdateFlag = 0x01;
    }
    else if (TURN_RIGHT == act_info->active)
    {
        anim_type = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
        run_data->clock_page = (run_data->clock_page + 1) % WEATHER_PAGE_SIZE;
        Serial.printf("[WEA-DBG] TURN_RIGHT: clock_page %d -> %d, last_clock_page=%d\n", 
                      (run_data->clock_page + WEATHER_PAGE_SIZE - 1) % WEATHER_PAGE_SIZE, 
                      run_data->clock_page, last_clock_page);
    }
    else if (TURN_LEFT == act_info->active)
    {
        anim_type = LV_SCR_LOAD_ANIM_MOVE_LEFT;
        int prev_page = run_data->clock_page;
        run_data->clock_page = (run_data->clock_page + WEATHER_PAGE_SIZE - 1) % WEATHER_PAGE_SIZE;
        Serial.printf("[WEA-DBG] TURN_LEFT: clock_page %d -> %d, last_clock_page=%d\n", 
                      prev_page, run_data->clock_page, last_clock_page);
    }

    // 界面刷新
    if (run_data->clock_page == 0)
    {
        // 天气页有动画和定时刷新，恢复 100ms 循环间隔
        APP_OBJ *app = sys->getAppByName(WEATHER_APP_NAME);
        if (app) {
            app->loop_interval_ms = 100;
        }
        // 数据变更守护：仅在首次渲染、页面切换、强制更新或天气数据更新时刷新UI
        if (!weather_ui_initialized || weather_data_changed || last_clock_page != 0 || run_data->coactusUpdateFlag == 0x01) {
            Serial.printf("[WEA-DBG] display_weather() called: ui_init=%d, data_chg=%d, last_page=%d, coactus=%d\n",
                          weather_ui_initialized, weather_data_changed, last_clock_page, run_data->coactusUpdateFlag);
            display_weather(run_data->wea, anim_type);
            weather_data_changed = false;
            weather_ui_initialized = true;
        }
        if (0x01 == run_data->coactusUpdateFlag || doDelayMillisTime(cfg_data.weatherUpdataInterval, &run_data->preWeatherMillis, false))
        {
            if (g_weather_now_req == NULL)
            {
                char api[128] = {0};
                snprintf(api, 128, WEATHER_LIVES_API,
                         cfg_data.tianqi_api_key.c_str(),
                         cfg_data.tianqi_city_code.c_str());
                g_weather_now_req = http_get_async(api, xTaskGetCurrentTaskHandle());
            }
            if (g_weather_daily_req == NULL)
            {
                char api[128] = {0};
                snprintf(api, 128, WEATHER_DALIY_FORECAST_API,
                         cfg_data.tianqi_api_key.c_str(),
                         cfg_data.tianqi_city_code.c_str());
                g_weather_daily_req = http_get_async(api, xTaskGetCurrentTaskHandle());
            }
        }

        if (0x01 == run_data->coactusUpdateFlag || doDelayMillisTime(cfg_data.timeUpdataInterval, &run_data->preTimeMillis, false))
        {
            // 尝试同步网络上的时钟
            if (g_weather_ntp_req == NULL)
            {
                g_weather_ntp_req = http_get_async(TIME_API, xTaskGetCurrentTaskHandle());
            }
        }
        else if (GET_SYS_MILLIS() - run_data->preLocalTimestamp > 400)
        {
            long long ts = get_timestamp();
            Serial.printf("[WEATHER] local refresh: GET_SYS_MILLIS()=%lu, preLocalTimestamp=%lu, diff=%lu\r\n",
                          GET_SYS_MILLIS(), run_data->preLocalTimestamp, GET_SYS_MILLIS() - run_data->preLocalTimestamp);
            updateTime_RTC(ts);
        }
        run_data->coactusUpdateFlag = 0x00; // 取消强制更新标志
        // 动画帧率守护：按固定帧率刷新太空人动画，避免每帧调用 lv_img_set_src 触发 LVGL 自激振荡
        if (GET_SYS_MILLIS() - last_space_anim_ms >= 300) {
            last_space_anim_ms = GET_SYS_MILLIS();
            display_space();
        }
    }
    else if (run_data->clock_page == 1)
    {
        // 页面切换守护：仅在首次渲染或切换到此页面时刷新曲线
        if (!weather_ui_initialized || last_clock_page != 1) {
            Serial.printf("[WEA-DBG] display_curve() called: ui_init=%d, last_page=%d\n",
                          weather_ui_initialized, last_clock_page);
            display_curve(run_data->wea.daily_max, run_data->wea.daily_min, anim_type);
            weather_ui_initialized = true;
        }
        // 折线图为静态页面，无动画无定时刷新，切换为永久阻塞，仅手势唤醒
        APP_OBJ *app = sys->getAppByName(WEATHER_APP_NAME);
        if (app) {
            app->loop_interval_ms = 0;
            Serial.printf("[WEA-DBG] curve page: loop_interval_ms = 0 (permanent block)\n");
        }
    }
    last_clock_page = run_data->clock_page;
}

static void weather_background_task(AppController *sys,
                                    const ImuAction *act_info)
{
    // 本函数为后台任务，主控制器会间隔一分钟调用此函数
    // 本函数尽量只调用"常驻数据",其他变量可能会因为生命周期的缘故已经释放
}

static int weather_exit_callback(void *param)
{
    weather_gui_del();

    if (g_weather_now_req)
    {
        g_weather_now_req->orphaned = true;
        g_weather_now_req = NULL;
    }
    if (g_weather_ntp_req)
    {
        g_weather_ntp_req->orphaned = true;
        g_weather_ntp_req = NULL;
    }
    if (g_weather_daily_req)
    {
        g_weather_daily_req->orphaned = true;
        g_weather_daily_req = NULL;
    }

    // 查杀异步任务
    if (run_data->xReturned_task_update == pdPASS)
    {
        vTaskDelete(run_data->xHandle_task_update);
    }
    run_data->xReturned_task_update = pdFAIL;

    // 释放运行数据
    if (NULL != run_data)
    {
        free(run_data);
        run_data = NULL;
    }
    return 0;
}

static void task_update(void *parameter)
{
    // 数据更新任务
    while (true)
    {
        if (run_data->update_type & UPDATE_WEATHER)
        {
            get_weather();
            run_data->update_type &= (~UPDATE_WEATHER);
        }
        if (run_data->update_type & UPDATE_TIME)
        {
            get_timestamp(TIME_API); // nowapi时间API
            run_data->update_type &= (~UPDATE_TIME);
        }
        if (run_data->update_type & UPDATE_DALIY_WEATHER)
        {
            get_daliyWeather(run_data->wea.daily_max, run_data->wea.daily_min);
            run_data->update_type &= (~UPDATE_DALIY_WEATHER);
        }
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

static void weather_message_handle(const char *from, const char *to,
                                   APP_MESSAGE_TYPE type, void *message,
                                   void *ext_info)
{
    switch (type)
    {
    case APP_MESSAGE_GET_PARAM:
    {
        char *param_key = (char *)message;
        if (!strcmp(param_key, "tianqi_url"))
        {
            snprintf((char *)ext_info, 128, "%s", cfg_data.tianqi_url.c_str());
        }
        else if (!strcmp(param_key, "tianqi_city_code"))
        {
            snprintf((char *)ext_info, 32, "%s", cfg_data.tianqi_city_code.c_str());
        }
        else if (!strcmp(param_key, "tianqi_api_key"))
        {
            snprintf((char *)ext_info, 40, "%s", cfg_data.tianqi_api_key.c_str());
        }
        else if (!strcmp(param_key, "weatherUpdataInterval"))
        {
            snprintf((char *)ext_info, 32, "%lu", cfg_data.weatherUpdataInterval);
        }
        else if (!strcmp(param_key, "timeUpdataInterval"))
        {
            snprintf((char *)ext_info, 32, "%lu", cfg_data.timeUpdataInterval);
        }
        else
        {
            snprintf((char *)ext_info, 32, "%s", "NULL");
        }
    }
    break;
    case APP_MESSAGE_SET_PARAM:
    {
        char *param_key = (char *)message;
        char *param_val = (char *)ext_info;
        if (!strcmp(param_key, "tianqi_url"))
        {
            cfg_data.tianqi_url = param_val;
        }
        else if (!strcmp(param_key, "tianqi_city_code"))
        {
            cfg_data.tianqi_city_code = param_val;
        }
        else if (!strcmp(param_key, "tianqi_api_key"))
        {
            cfg_data.tianqi_api_key = param_val;
        }
        else if (!strcmp(param_key, "weatherUpdataInterval"))
        {
            cfg_data.weatherUpdataInterval = atol(param_val);
        }
        else if (!strcmp(param_key, "timeUpdataInterval"))
        {
            cfg_data.timeUpdataInterval = atol(param_val);
        }
    }
    break;
    case APP_MESSAGE_READ_CFG:
    {
        read_config(&cfg_data);
    }
    break;
    case APP_MESSAGE_WRITE_CFG:
    {
        write_config(&cfg_data);
    }
    break;
    default:
        break;
    }
}

APP_OBJ weather_app = {WEATHER_APP_NAME, &app_weather, "",
                       weather_init, weather_process, weather_background_task,
                       weather_exit_callback, weather_message_handle};