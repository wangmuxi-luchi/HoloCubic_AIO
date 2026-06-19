#ifndef HTTPCLIENT_STUB_H
#define HTTPCLIENT_STUB_H

#include "Arduino.h"
#include "IPAddress.h"

#define HTTP_CODE_OK 200
#define HTTP_CODE_MOVED_PERMANENTLY 301

class HTTPClient
{
public:
    bool begin(const char *url) { (void)url; return true; }
    bool begin(String url) { return true; }
    int GET() { return 200; }
    int POST(const char *body) { (void)body; return 200; }
    int POST(String body) { return 200; }
    int sendRequest(const char *method, const char *body) { (void)method; (void)body; return 200; }
    int getSize() { return 0; }
    String getString() { return ""; }
    String errorToString(int error) { (void)error; return ""; }
    void setTimeout(uint16_t timeout) { (void)timeout; }
    void addHeader(const char *name, const char *value) { (void)name; (void)value; }
    void end() {}
    bool connected() { return false; }
};

#endif