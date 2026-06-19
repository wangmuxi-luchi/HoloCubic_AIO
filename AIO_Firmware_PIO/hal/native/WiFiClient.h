#ifndef WIFI_CLIENT_STUB_H
#define WIFI_CLIENT_STUB_H

#include <stdint.h>
#include <stddef.h>

class WiFiClient
{
public:
    bool connect(const char *host, uint16_t port) { (void)host; (void)port; return false; }
    void stop() {}
    int available() { return 0; }
    int read() { return -1; }
    int read(uint8_t *buf, size_t size) { (void)buf; (void)size; return 0; }
    size_t write(const uint8_t *buf, size_t size) { (void)buf; (void)size; return 0; }
    operator bool() const { return false; }
};

#endif /* WIFI_CLIENT_STUB_H */