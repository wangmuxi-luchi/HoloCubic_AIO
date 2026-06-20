#include "network_async.h"
#include <HTTPClient.h>
#include <Arduino.h>

static void http_request_task(void *param)
{
    HttpRequest *req = (HttpRequest *)param;

    HTTPClient http;
    http.setTimeout(5000);
    if (http.begin(req->url))
    {
        req->http_code = http.GET();
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

    __sync_synchronize();
    req->done = true;

    if (req->notify_task)
    {
        xTaskNotifyGive(req->notify_task);
    }

    vTaskDelete(NULL);
}

HttpRequest *http_get_async(const char *url, TaskHandle_t notify_task)
{
    HttpRequest *req = (HttpRequest *)pvPortMalloc(sizeof(HttpRequest));
    if (!req)
    {
        return NULL;
    }

    memset(req, 0, sizeof(HttpRequest));
    strncpy(req->url, url, sizeof(req->url) - 1);
    req->url[sizeof(req->url) - 1] = '\0';
    req->notify_task = notify_task;
    req->done = false;
    req->http_code = -1;

    BaseType_t ret = xTaskCreate(
        http_request_task,
        "http_async",
        4 * 1024,
        req,
        2,
        NULL);

    if (ret != pdPASS)
    {
        vPortFree(req);
        return NULL;
    }

    return req;
}