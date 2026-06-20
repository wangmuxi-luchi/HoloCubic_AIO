#include "SD.h"
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

SDClass SD;

extern "C" {
    __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename, unsigned long nSize);
}

static char g_sdBasePath[512] = "";

static void computeSdBasePath()
{
    if (g_sdBasePath[0] != '\0') return;
    char exePath[512];
    GetModuleFileNameA(NULL, exePath, sizeof(exePath));
    char *lastSep = strrchr(exePath, '\\');
    if (lastSep) *lastSep = '\0';
    lastSep = strrchr(exePath, '\\');
    if (lastSep) *lastSep = '\0';
    lastSep = strrchr(exePath, '\\');
    if (lastSep) *lastSep = '\0';
    lastSep = strrchr(exePath, '\\');
    if (lastSep) *lastSep = '\0';
    snprintf(g_sdBasePath, sizeof(g_sdBasePath), "%s\\sim_data\\sd", exePath);
}

static void toNativePath(const char *virtPath, char *nativePath, size_t size)
{
    computeSdBasePath();
    if (virtPath[0] == '/') virtPath++;
    snprintf(nativePath, size, "%s\\%s", g_sdBasePath, virtPath);
    for (char *p = nativePath; *p; ++p)
        if (*p == '/') *p = '\\';
}

bool SDClass::exists(const char *path)
{
    char nativePath[512];
    toNativePath(path, nativePath, sizeof(nativePath));
    printf("[SD] exists(%s) -> %s\n", path, nativePath);
    struct stat st;
    int result = stat(nativePath, &st);
    printf("[SD] exists result=%d\n", result);
    return result == 0;
}

File SDClass::open(const char *path, const char *mode)
{
    char nativePath[512];
    toNativePath(path, nativePath, sizeof(nativePath));
    printf("[SD] open(%s, %s) -> %s\n", path, mode, nativePath);
    File f(nativePath, mode);
    printf("[SD] open result=%d\n", (bool)f);
    return f;
}