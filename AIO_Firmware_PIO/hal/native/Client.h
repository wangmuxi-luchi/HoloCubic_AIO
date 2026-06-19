#ifndef CLIENT_STUB_H
#define CLIENT_STUB_H

#include "Arduino.h"
#include "Print.h"
#include "IPAddress.h"

class Client : public Print
{
public:
    virtual int connect(const char *host, uint16_t port) { return 0; }
    virtual int connect(uint8_t *ip, uint16_t port) { return 0; }
    virtual int connect(IPAddress ip, uint16_t port) { return 0; }
    virtual size_t write(uint8_t c) override { return 0; }
    virtual size_t write(const uint8_t *buf, size_t size) override { return 0; }
    virtual int available() { return 0; }
    virtual int read() { return -1; }
    virtual int read(uint8_t *buf, size_t size) { return 0; }
    virtual int peek() { return -1; }
    virtual void flush() { }
    virtual void stop() { }
    virtual uint8_t connected() { return 0; }
    virtual int setSocketOption(int, int, int) { return 0; }
};

#endif