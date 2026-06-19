#ifndef WEB_SERVER_STUB_H
#define WEB_SERVER_STUB_H

#include <stdint.h>
#include "WiFi.h"

#define HTTP_GET  0
#define HTTP_POST 1
#define CONTENT_LENGTH_UNKNOWN ((size_t)-1)

class WebServer
{
public:
    WebServer() {}
    WebServer(int port) { (void)port; }
    void on(const char *uri, void (*handler)(void)) { (void)uri; (void)handler; }
    void on(const char *uri, int method, void (*handler)(void)) { (void)uri; (void)method; (void)handler; }
    void on(const char *uri, int method, void (*handler)(void), void (*uploadHandler)(void)) { (void)uri; (void)method; (void)handler; (void)uploadHandler; }
    void begin(void) {}
    void handleClient(void) {}
    void send(int code, const char *type, const char *content) { (void)code; (void)type; (void)content; }
    void send(int code) { (void)code; }
    void sendHeader(const char *name, const char *value, bool first = false) { (void)name; (void)value; (void)first; }
    void setContentLength(size_t len) { (void)len; }
    void sendContent(const char *content) { (void)content; }
    void sendContent(const String &content) { (void)content; }
    void stop(void) {}
    void close(void) {}
    WiFiClient &client(void) { static WiFiClient c; return c; }
    String arg(const char *name) { (void)name; return ""; }
    String arg(int i) { (void)i; return ""; }
};

#endif /* WEB_SERVER_STUB_H */