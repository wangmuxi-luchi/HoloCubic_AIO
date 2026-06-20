#ifndef NETWORK_ASYNC_H
#define NETWORK_ASYNC_H

#include <FreeRTOS.h>
#include <task.h>

struct HttpRequest
{
    char url[256];
    char response[1024];
    int http_code;
    TaskHandle_t notify_task;
    bool done;
};

HttpRequest *http_get_async(const char *url, TaskHandle_t notify_task);

#endif