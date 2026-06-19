#ifndef FS_STUB_H
#define FS_STUB_H

#include "Arduino.h"
#include <stdint.h>

namespace fs
{
    class File
    {
    public:
        File() {}
        operator bool() const { return false; }
        size_t read(uint8_t *buf, size_t len) { return 0; }
        size_t write(const uint8_t *buf, size_t len) { return 0; }
        void close() {}
        int available() { return 0; }
        int read() { return -1; }
        int peek() { return -1; }
        void seek(uint32_t pos) {}
        size_t position() { return 0; }
        size_t size() { return 0; }
        const char *name() { return ""; }
        bool isDirectory() { return false; }
        File openNextFile(const char *mode = NULL) { return File(); }
        unsigned long getLastWrite() { return 0; }
    };

    class FS
    {
    public:
        bool begin() { return true; }
        void end() {}
        File open(const char *path, const char *mode) { return File(); }
        bool exists(const char *path) { return false; }
    };
}

typedef fs::File File;

#endif