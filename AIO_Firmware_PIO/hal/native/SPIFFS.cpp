#include "SPIFFS.h"

extern "C" {
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
}

const char *SPIFFSClass::_basePath = "sim_data/spiffs";

void computeBasePath()
{
    static char exePath[512];
    static bool computed = false;
    if (computed) return;
    computed = true;

    GetModuleFileNameA(NULL, exePath, sizeof(exePath));
    char *lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *lastSlash = '\0';
    // Remove .pio\build\AIO_native_sim from the path
    lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *lastSlash = '\0';
    lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *lastSlash = '\0';
    lastSlash = strrchr(exePath, '\\');
    if (lastSlash) *lastSlash = '\0';

    static char buf[512];
    snprintf(buf, sizeof(buf), "%s\\sim_data\\spiffs", exePath);
    SPIFFSClass::_basePath = buf;
}

SPIFFSClass SPIFFS;