#ifndef SD_STUB_H
#define SD_STUB_H

#include "FS.h"

class SDClass
{
public:
    bool begin(uint8_t cs = 0, uint32_t freq = 0) { (void)cs; (void)freq; return true; }
    File open(const char *path, const char *mode = "r") { (void)path; (void)mode; return File(); }
    bool exists(const char *path) { (void)path; return false; }
};

extern SDClass SD;

#endif /* SD_STUB_H */