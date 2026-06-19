#ifndef WIFI_STUB_H
#define WIFI_STUB_H

#include <stdint.h>
#include "IPAddress.h"
#include "Client.h"

#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
#define WL_IDLE_STATUS 0
#define WL_NO_SSID_AVAIL 1
#define WL_SCAN_COMPLETED 2
#define WL_CONNECT_FAILED 4
#define WL_CONNECTION_LOST 5

#define WIFI_MODE_STA  1
#define WIFI_MODE_AP   2
#define WIFI_MODE_APSTA 3

class WiFiClass
{
public:
    int status() { return WL_CONNECTED; }
    void begin(const char *ssid, const char *password) {}
    void disconnect() {}
    IPAddress localIP() { return IPAddress(127, 0, 0, 1); }
    IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
    int getMode() { return WIFI_MODE_STA; }
};

extern WiFiClass WiFi;

class WiFiClient : public Client
{
public:
    WiFiClient() {}
    int connect(IPAddress ip, uint16_t port) { (void)ip; (void)port; return 1; }
    int connect(const char *host, uint16_t port) { (void)host; (void)port; return 1; }
    size_t write(uint8_t c) override { (void)c; return 1; }
    size_t write(const uint8_t *buf, size_t size) override { (void)buf; return size; }
    int available() { return 0; }
    int read() { return -1; }
    int read(uint8_t *buf, size_t size) { (void)buf; (void)size; return 0; }
    int peek() { return -1; }
    void flush() {}
    void stop() {}
    uint8_t connected() { return 1; }
    operator bool() { return true; }
    void println(const char *s = "") { (void)s; }
    void println(int n) { (void)n; }
    void print(const char *s) { (void)s; }
    void print(int n) { (void)n; }
};

#endif