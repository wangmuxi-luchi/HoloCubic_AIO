#include "network_async.h"
#include "sys/app_controller.h"
#include <HTTPClient.h>
#include <Arduino.h>

extern AppController *app_controller;

static void http_request_task(void *param)
{
    HttpRequest *req = (HttpRequest *)param;

    Serial.printf("[HTTP_ASYNC] task started, url=%s\n", req->url);
    Serial.flush();

    HTTPClient http;
    http.setTimeout(5000);

    Serial.printf("[HTTP_ASYNC] calling http.begin()...\n");
    Serial.flush();
    bool begin_ok = http.begin(req->url);
    Serial.printf("[HTTP_ASYNC] http.begin() returned %d\n", (int)begin_ok);
    Serial.flush();

    if (begin_ok)
    {
        if (req->headers[0] != '\0')
        {
            char *saveptr;
            char *token = strtok_r(req->headers, "\r\n", &saveptr);
            while (token)
            {
                char *colon = strchr(token, ':');
                if (colon)
                {
                    *colon = '\0';
                    char *name = token;
                    char *value = colon + 1;
                    while (*value == ' ') value++;
                    http.addHeader(name, value);
                    *colon = ':';
                }
                token = strtok_r(NULL, "\r\n", &saveptr);
            }
        }

        Serial.printf("[HTTP_ASYNC] calling http.GET()...\n");
        req->http_code = http.GET();
        Serial.printf("[HTTP_ASYNC] http.GET() returned %d\n", req->http_code);
        if (req->http_code > 0)
        {
            String body = http.getString();
            strncpy(req->response, body.c_str(), sizeof(req->response) - 1);
            req->response[sizeof(req->response) - 1] = '\0';
        }
        http.end();
    }
    else
    {
        req->http_code = -1;
    }

    // 在设置 done 之前，先把所有需要从 req 读取的值保存到本地
    // 防止主线程在 done=true 后立即 vPortFree(req) 导致 use-after-free
    int http_code = req->http_code;
    bool orphaned = req->orphaned;
    TaskHandle_t notify = req->notify_task;

    __sync_synchronize();
    req->done = true;

    Serial.printf("[HTTP_ASYNC] task done, http_code=%d, orphaned=%d\n", http_code, (int)orphaned);
    Serial.flush();

    if (!orphaned && notify)
    {
        xTaskNotifyGive(notify);
    }

    if (orphaned)
    {
        vPortFree(req);
    }

    vTaskDelete(NULL);
}

HttpRequest *http_get_async(const char *url, TaskHandle_t notify_task, const char *headers)
{
    Serial.printf("[HTTP_ASYNC] http_get_async, url=%s\n", url);
    Serial.flush();

    if (app_controller) {
        app_controller->wifi_keep_alive();
    }

    HttpRequest *req = (HttpRequest *)pvPortMalloc(sizeof(HttpRequest));
    if (!req)
    {
        return NULL;
    }
    Serial.printf("[HTTP_ASYNC] http_get_async malloc, req=%p, url=%s\n", req, url);
    Serial.flush();

    memset(req, 0, sizeof(HttpRequest));
    strncpy(req->url, url, sizeof(req->url) - 1);
    req->url[sizeof(req->url) - 1] = '\0';
    if (headers && headers[0] != '\0')
    {
        strncpy(req->headers, headers, sizeof(req->headers) - 1);
        req->headers[sizeof(req->headers) - 1] = '\0';
    }
    req->notify_task = notify_task;
    req->done = false;
    req->orphaned = false;
    req->http_code = -1;
    Serial.printf("[HTTP_ASYNC] http_get_async memset done\n");
    Serial.flush();

    char task_name[16];
    snprintf(task_name, sizeof(task_name), "http_%04X", (uint16_t)(uintptr_t)req);
    BaseType_t ret = xTaskCreate(
        http_request_task,
        task_name,
        3 * 1024,
        req,
        1,
        NULL);

    if (ret != pdPASS)
    {
        Serial.printf("[HTTP_ASYNC] %s xTaskCreate FAILED, ret=%d, heap=%u\n", task_name, (int)ret, xPortGetFreeHeapSize());
        Serial.flush();
        vPortFree(req);
        return NULL;
    }
    Serial.printf("[HTTP_ASYNC] %s task created, ret=%d\n", task_name, (int)ret);
    Serial.flush();

    return req;
}