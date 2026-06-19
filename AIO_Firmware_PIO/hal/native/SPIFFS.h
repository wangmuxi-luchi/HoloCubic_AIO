#ifndef SPIFFS_STUB_H
#define SPIFFS_STUB_H

#include <stdbool.h>
#include "FS.h"

#ifdef __cplusplus

class SPIFFSClass
{
public:
    bool begin(bool formatOnFail = false) { (void)formatOnFail; return true; }
    void end() {}
    bool format() { return true; }
    File open(const char *path) { return File(); }
    File open(const char *path, const char *mode) { (void)mode; return File(); }
};

extern SPIFFSClass SPIFFS;

#endif /* __cplusplus */

#endif /* SPIFFS_STUB_H */