#include "ESP32FtpServer.h"
#include "SD.h"
#include "serial_utils.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

FtpServer::FtpServer()
    : _ctrlServer(21), _dataServer(0),
      _authenticated(false), _dataPassive(false),
      _recvLen(0), _dataPort(0), _lastActivity(0)
{
    memset(_user, 0, sizeof(_user));
    memset(_pass, 0, sizeof(_pass));
    memset(_cwd, 0, sizeof(_cwd));
    memset(_rnfr, 0, sizeof(_rnfr));
    _cwd[0] = '/';
}

void FtpServer::begin(String uname, String pword)
{
    strncpy(_user, uname.c_str(), sizeof(_user) - 1);
    strncpy(_pass, pword.c_str(), sizeof(_pass) - 1);
    _ctrlServer.begin();
    CreateDirectoryA(sd_get_base_path(), NULL);
    printf("[FtpServer] Started on port 21\n");
}

void FtpServer::_resp(const char *code, const char *msg)
{
    _responseBuffer += code;
    _responseBuffer += " ";
    _responseBuffer += msg;
    _responseBuffer += "\r\n";
}

void FtpServer::_sendResponse()
{
    if (!_responseBuffer.empty())
    {
        _ctrlServer.write(_responseBuffer.c_str(), (int)_responseBuffer.size());
        _responseBuffer.clear();
    }
}

void FtpServer::handleFTP()
{
    if (!_ctrlServer.isRunning())
        return;

    if (!_ctrlServer.isClientConnected())
    {
        if (_ctrlServer.acceptClient())
        {
            _recvLen = 0;
            _authenticated = false;
            _responseBuffer.clear();
            _lastActivity = (int)time(nullptr);
            _resp("220", "HoloCubic FTP Server Ready");
            _sendResponse();
            printf("[FtpServer] Client connected\n");
        }
        return;
    }

    char buf[256];
    int n = _ctrlServer.read(buf, (int)sizeof(buf) - 1);
    if (n <= 0)
    {
        int now = (int)time(nullptr);
        if (now - _lastActivity > 300)
        {
            printf("[FtpServer] Timeout, disconnecting\n");
            _ctrlServer.disconnectClient();
        }
        return;
    }

    buf[n] = '\0';
    _lastActivity = (int)time(nullptr);

    strncat(_recvBuf, buf, sizeof(_recvBuf) - _recvLen - 1);
    _recvLen += n;

    char *lineEnd;
    while ((lineEnd = strstr(_recvBuf, "\r\n")) != nullptr)
    {
        *lineEnd = '\0';
        char *cmd = _recvBuf;

        printf("[FtpServer] -> %s\n", cmd);

        char *arg = strchr(cmd, ' ');
        if (arg)
        {
            *arg = '\0';
            arg++;
        }

        _processCommand(cmd, arg);

        int remaining = _recvLen - (int)(lineEnd - _recvBuf) - 2;
        if (remaining > 0)
        {
            memmove(_recvBuf, lineEnd + 2, remaining);
            _recvLen = remaining;
            _recvBuf[_recvLen] = '\0';
        }
        else
        {
            _recvLen = 0;
            _recvBuf[0] = '\0';
            break;
        }
    }

    _sendResponse();
}

void FtpServer::_processCommand(const char *cmd, const char *arg)
{
    if (!cmd)
    {
        _resp("500", "Unknown command");
        return;
    }

    if (strcmp(cmd, "USER") == 0)
    {
        if (arg && strcmp(arg, _user) == 0)
        {
            _resp("331", "Password required");
        }
        else
        {
            _resp("530", "Invalid username");
        }
    }
    else if (strcmp(cmd, "PASS") == 0)
    {
        if (arg && strcmp(arg, _pass) == 0)
        {
            _authenticated = true;
            _resp("230", "User logged in");
        }
        else
        {
            _resp("530", "Login incorrect");
        }
    }
    else if (!_authenticated)
    {
        _resp("530", "Not logged in");
    }
    else if (strcmp(cmd, "SYST") == 0)
    {
        _resp("215", "UNIX Type: L8");
    }
    else if (strcmp(cmd, "FEAT") == 0)
    {
        _resp("211", "Features:");
        _resp("211", " SIZE");
        _resp("211", " MDTM");
        _resp("211", "End");
    }
    else if (strcmp(cmd, "TYPE") == 0)
    {
        _resp("200", "Type set to I");
    }
    else if (strcmp(cmd, "PWD") == 0)
    {
        char msg[512];
        snprintf(msg, sizeof(msg), "\"%s\" is current directory", _cwd);
        _resp("257", msg);
    }
    else if (strcmp(cmd, "CWD") == 0)
    {
        if (!arg || arg[0] == '\0')
        {
            _resp("550", "No directory specified");
            return;
        }

        std::string newPath;
        if (arg[0] == '/')
        {
            newPath = arg;
        }
        else
        {
            newPath = _cwd;
            if (newPath != "/")
                newPath += "/";
            newPath += arg;
        }

        while (newPath.find("/./") != std::string::npos)
        {
            size_t p = newPath.find("/./");
            newPath.erase(p, 2);
        }
        while (newPath.find("//") != std::string::npos)
        {
            size_t p = newPath.find("//");
            newPath.erase(p, 1);
        }

        std::string sdPath = sd_get_base_path() + newPath;
        if (sdPath.back() != '/')
            sdPath += "/";

        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA((sdPath + "*").c_str(), &fd);
        if (h == INVALID_HANDLE_VALUE)
        {
            _resp("550", "Directory not found");
        }
        else
        {
            FindClose(h);
            strncpy(_cwd, newPath.c_str(), sizeof(_cwd) - 1);
            _resp("250", "Directory changed");
        }
    }
    else if (strcmp(cmd, "CDUP") == 0)
    {
        if (strcmp(_cwd, "/") == 0)
        {
            _resp("250", "Already at root");
        }
        else
        {
            char *lastSlash = strrchr(_cwd, '/');
            if (lastSlash == _cwd)
            {
                _cwd[1] = '\0';
            }
            else if (lastSlash)
            {
                *lastSlash = '\0';
            }
            _resp("250", "Directory changed");
        }
    }
    else if (strcmp(cmd, "MKD") == 0)
    {
        if (!arg)
        {
            _resp("550", "No directory name");
            return;
        }
        std::string sdPath = sd_get_base_path() + std::string(_cwd);
        if (sdPath.back() != '/')
            sdPath += "/";
        sdPath += arg;
        if (CreateDirectoryA(sdPath.c_str(), NULL))
        {
            char msg[512];
            snprintf(msg, sizeof(msg), "\"%s\" created", arg);
            _resp("257", msg);
        }
        else
        {
            _resp("550", "Create directory failed");
        }
    }
    else if (strcmp(cmd, "RMD") == 0)
    {
        if (!arg)
        {
            _resp("550", "No directory name");
            return;
        }
        std::string sdPath = sd_get_base_path() + std::string(_cwd);
        if (sdPath.back() != '/')
            sdPath += "/";
        sdPath += arg;
        if (RemoveDirectoryA(sdPath.c_str()))
        {
            _resp("250", "Directory removed");
        }
        else
        {
            _resp("550", "Remove directory failed");
        }
    }
    else if (strcmp(cmd, "DELE") == 0)
    {
        if (!arg)
        {
            _resp("550", "No file name");
            return;
        }
        std::string sdPath = sd_get_base_path() + std::string(_cwd);
        if (sdPath.back() != '/')
            sdPath += "/";
        sdPath += arg;
        if (DeleteFileA(sdPath.c_str()))
        {
            _resp("250", "File deleted");
        }
        else
        {
            _resp("550", "Delete failed");
        }
    }
    else if (strcmp(cmd, "SIZE") == 0)
    {
        if (!arg)
        {
            _resp("550", "No file name");
            return;
        }
        std::string sdPath = sd_get_base_path() + std::string(_cwd);
        if (sdPath.back() != '/')
            sdPath += "/";
        sdPath += arg;

        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (GetFileAttributesExA(sdPath.c_str(), GetFileExInfoStandard, &attr))
        {
            ULONGLONG fileSize = ((ULONGLONG)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
            char msg[64];
            snprintf(msg, sizeof(msg), "%llu", fileSize);
            _resp("213", msg);
        }
        else
        {
            _resp("550", "File not found");
        }
    }
    else if (strcmp(cmd, "MDTM") == 0)
    {
        if (!arg)
        {
            _resp("550", "No file name");
            return;
        }
        std::string sdPath = sd_get_base_path() + std::string(_cwd);
        if (sdPath.back() != '/')
            sdPath += "/";
        sdPath += arg;

        WIN32_FILE_ATTRIBUTE_DATA attr;
        if (GetFileAttributesExA(sdPath.c_str(), GetFileExInfoStandard, &attr))
        {
            SYSTEMTIME st;
            FileTimeToSystemTime(&attr.ftLastWriteTime, &st);
            char msg[64];
            snprintf(msg, sizeof(msg), "%04d%02d%02d%02d%02d%02d",
                     st.wYear, st.wMonth, st.wDay,
                     st.wHour, st.wMinute, st.wSecond);
            _resp("213", msg);
        }
        else
        {
            _resp("550", "File not found");
        }
    }
    else if (strcmp(cmd, "PASV") == 0)
    {
        serial_printf("[FTP_SRV] PASV: received, creating data server...\n");
        _closeData();
        _responseBuffer.clear();  // _closeData() 追加了 226，但 PASV 不需要

        _dataServer = NativeServer(0);
        if (!_dataServer.begin())
        {
            serial_printf("[FTP_SRV] PASV: data server begin() FAILED\n");
            _resp("425", "Cannot open data connection");
            return;
        }

        sockaddr_in addr;
        int addrLen = sizeof(addr);
        getsockname(_dataServer.getListenSocket(), (sockaddr *)&addr, &addrLen);
        _dataPort = ntohs(addr.sin_port);

        serial_printf("[FTP_SRV] PASV: data port=%d\n", _dataPort);
        char msg[128];
        snprintf(msg, sizeof(msg), "Entering Passive Mode (127,0,0,1,%d,%d)",
                 _dataPort >> 8, _dataPort & 0xFF);
        serial_printf("[FTP_SRV] PASV: response=%s\n", msg);
        _resp("227", msg);
        _dataPassive = true;
    }
    else if (strcmp(cmd, "LIST") == 0 || strcmp(cmd, "NLST") == 0)
    {
        if (!_dataConnect())
            return;
        _doList(cmd[0] == 'L' ? arg : nullptr);
        _closeData();
    }
    else if (strcmp(cmd, "RETR") == 0)
    {
        if (!_dataConnect())
            return;
        _doRetrieve(arg);
        _closeData();
    }
    else if (strcmp(cmd, "STOR") == 0)
    {
        if (!_dataConnect())
            return;
        _doStore(arg);
        _closeData();
    }
    else if (strcmp(cmd, "RNFR") == 0)
    {
        if (!arg)
        {
            _resp("550", "No file name");
            return;
        }
        strncpy(_rnfr, arg, sizeof(_rnfr) - 1);
        _resp("350", "Ready for RNTO");
    }
    else if (strcmp(cmd, "RNTO") == 0)
    {
        if (!arg || _rnfr[0] == '\0')
        {
            _resp("503", "RNFR required first");
            return;
        }
        std::string sdFrom = sd_get_base_path() + std::string(_cwd);
        if (sdFrom.back() != '/')
            sdFrom += "/";
        sdFrom += _rnfr;

        std::string sdTo = sd_get_base_path() + std::string(_cwd);
        if (sdTo.back() != '/')
            sdTo += "/";
        sdTo += arg;

        if (MoveFileA(sdFrom.c_str(), sdTo.c_str()))
        {
            _resp("250", "File renamed");
        }
        else
        {
            _resp("550", "Rename failed");
        }
        _rnfr[0] = '\0';
    }
    else if (strcmp(cmd, "QUIT") == 0)
    {
        _resp("221", "Goodbye");
        _sendResponse();
        _ctrlServer.disconnectClient();
        printf("[FtpServer] Client disconnected\n");
    }
    else if (strcmp(cmd, "NOOP") == 0)
    {
        _resp("200", "NOOP ok");
    }
    else
    {
        char msg[128];
        snprintf(msg, sizeof(msg), "'%s': command not understood", cmd);
        _resp("502", msg);
    }
}

bool FtpServer::_dataConnect()
{
    if (!_dataPassive)
    {
        _resp("425", "Use PASV first");
        return false;
    }

    _dataServer.acceptClient();
    if (!_dataServer.isClientConnected())
    {
        _resp("425", "Cannot open data connection");
        _dataPassive = false;
        return false;
    }

    _resp("150", "Opening data connection");
    _sendResponse();
    return true;
}

void FtpServer::_closeData()
{
    _dataServer.stop();
    _dataPassive = false;
    _resp("226", "Transfer complete");
}

void FtpServer::_doList(const char *arg)
{
    std::string sdPath = sd_get_base_path();
    std::string searchPath = sdPath + std::string(_cwd);
    if (searchPath.back() != '/')
        searchPath += "/";
    searchPath += "*";

    std::string listing;
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(searchPath.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
                continue;

            SYSTEMTIME st;
            FileTimeToSystemTime(&fd.ftLastWriteTime, &st);

            char permissions[11];
            strcpy(permissions, (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "drwxr-xr-x" : "-rw-r--r--");

            ULONGLONG fileSize = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;

            char line[512];
            const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
            snprintf(line, sizeof(line), "%s 1 owner group %12llu %s %2d %02d:%02d %s\r\n",
                     permissions,
                     fileSize,
                     months[st.wMonth - 1],
                     st.wDay,
                     st.wHour,
                     st.wMinute,
                     fd.cFileName);
            listing += line;
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }

    if (listing.empty())
    {
        listing = "\r\n";
    }

    _dataServer.write(listing.c_str(), (int)listing.size());
}

void FtpServer::_doRetrieve(const char *arg)
{
    if (!arg)
    {
        _resp("550", "No file name");
        return;
    }

    std::string sdPath = sd_get_base_path() + std::string(_cwd);
    if (sdPath.back() != '/')
        sdPath += "/";
    sdPath += arg;

    FILE *f = fopen(sdPath.c_str(), "rb");
    if (!f)
    {
        _resp("550", "File not found");
        return;
    }

    char buf[4096];
    int n;
    while ((n = (int)fread(buf, 1, sizeof(buf), f)) > 0)
    {
        _dataServer.write(buf, n);
    }
    fclose(f);
}

void FtpServer::_doStore(const char *arg)
{
    if (!arg)
    {
        _resp("550", "No file name");
        return;
    }

    std::string sdPath = sd_get_base_path() + std::string(_cwd);
    if (sdPath.back() != '/')
        sdPath += "/";
    sdPath += arg;

    FILE *f = fopen(sdPath.c_str(), "wb");
    if (!f)
    {
        _resp("550", "Cannot create file");
        return;
    }

    char buf[4096];
    int n;
    int timeout = 0;
    while (timeout < 500)
    {
        n = _dataServer.read(buf, (int)sizeof(buf));
        if (n > 0)
        {
            fwrite(buf, 1, n, f);
            timeout = 0;  // 收到数据，重置超时
        }
        else if (n == 0)
        {
            break;  // 连接关闭
        }
        else
        {
            Sleep(10);  // 无数据，等待 10ms
            timeout++;
        }
    }
    fclose(f);
}