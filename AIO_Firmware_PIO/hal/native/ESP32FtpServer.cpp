#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>

#include "ESP32FtpServer.h"
#include "SD.h"
#include "serial_utils.h"
#include <WiFi.h>
#include <WiFiClient.h>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <utility>
#include <string>

// Global FTP server objects (same as real implementation)
WiFiServer ftpServer(FTP_CTRL_PORT);
WiFiServer dataServer(FTP_DATA_PORT_PASV);

static bool _authenticated = false;

void FtpServer::begin(const String &uname, const String &pword)
{
    _FTP_USER = uname;
    _FTP_PASS = pword;
    cwdName[0] = '/';
    cwdName[1] = '\0';

    ftpServer.begin();
    CreateDirectoryA(sd_get_base_path(), NULL);
    printf("[FtpServer] Started on port %d\n", FTP_CTRL_PORT);
}

void FtpServer::handleFTP()
{
    std::string _responseBuffer;

    // 先检查是否有新客户端等待连接（即使当前客户端仍连接）
    if (ftpServer.hasClient())
    {
        client.stop();
        WiFiClient newClient = ftpServer.available();
        client = std::move(newClient);
        iCL = 0;
        _authenticated = false;
        cmdLine[0] = '\0';
        dataPassiveConn = false;
        rnfrCmd = false;
        millisEndConnection = (uint32_t)time(nullptr) + FTP_TIME_OUT * 60000;
        _responseBuffer += "220 HoloCubic FTP Server Ready\r\n";
        client.print(_responseBuffer.c_str());
        printf("[FtpServer] Client connected (hasClient)\n");
        return;
    }

    // Accept new clients
    if (!client || !client.connected())
    {
        WiFiClient newClient = ftpServer.available();
        if (newClient)
        {
            client = std::move(newClient);
            iCL = 0;
            _authenticated = false;
            cmdLine[0] = '\0';
            dataPassiveConn = false;
            rnfrCmd = false;
            millisEndConnection = (uint32_t)time(nullptr) + FTP_TIME_OUT * 60000;
            _responseBuffer += "220 HoloCubic FTP Server Ready\r\n";
            client.print(_responseBuffer.c_str());
            printf("[FtpServer] Client connected\n");
        }
        else
        {
            return;
        }
    }

    // 超时断连检查
    if ((uint32_t)time(nullptr) > millisEndConnection)
    {
        _responseBuffer += "421 Timeout\r\n";
        client.print(_responseBuffer.c_str());
        client.stop();
        printf("[FtpServer] Timeout, disconnecting\n");
        return;
    }

    // Read commands from client
    while (client.available())
    {
        char c = (char)client.read();
        if (c == '\n')
        {
            if (iCL > 0 && cmdLine[iCL - 1] == '\r')
                cmdLine[iCL - 1] = '\0';
            else
                cmdLine[iCL] = '\0';

            printf("[FtpServer] -> %s\n", cmdLine);

            millisEndConnection = (uint32_t)time(nullptr) + FTP_TIME_OUT * 60000;

            char *cmd = cmdLine;
            char *arg = strchr(cmd, ' ');
            if (arg)
            {
                *arg = '\0';
                arg++;
                while (*arg == ' ') arg++;
            }
            parameters = arg;

            // === Command processing ===
            if (strcmp(cmd, "USER") == 0)
            {
                if (arg && strcmp(arg, _FTP_USER.c_str()) == 0)
                    _responseBuffer += "331 Password required\r\n";
                else
                    _responseBuffer += "530 Invalid username\r\n";
            }
            else if (strcmp(cmd, "PASS") == 0)
            {
                if (arg && strcmp(arg, _FTP_PASS.c_str()) == 0)
                {
                    _authenticated = true;
                    _responseBuffer += "230 User logged in\r\n";
                }
                else
                    _responseBuffer += "530 Login incorrect\r\n";
            }
            else if (!_authenticated)
            {
                _responseBuffer += "530 Not logged in\r\n";
            }
            else if (strcmp(cmd, "SYST") == 0)
            {
                _responseBuffer += "215 UNIX Type: L8\r\n";
            }
            else if (strcmp(cmd, "FEAT") == 0)
            {
                _responseBuffer += "211-Features:\r\n";
                _responseBuffer += " SIZE\r\n";
                _responseBuffer += " MDTM\r\n";
                _responseBuffer += "211 End\r\n";
            }
            else if (strcmp(cmd, "TYPE") == 0)
            {
                _responseBuffer += "200 Type set to I\r\n";
            }
            else if (strcmp(cmd, "PWD") == 0)
            {
                char msg[512];
                snprintf(msg, sizeof(msg), "257 \"%s\" is current directory\r\n", cwdName);
                _responseBuffer += msg;
            }
            else if (strcmp(cmd, "CWD") == 0)
            {
                if (!arg || arg[0] == '\0')
                {
                    _responseBuffer += "550 No directory specified\r\n";
                }
                else
                {
                    std::string newPath;
                    if (arg[0] == '/')
                        newPath = arg;
                    else
                    {
                        newPath = cwdName;
                        if (newPath != "/") newPath += "/";
                        newPath += arg;
                    }

                    // 路径规范化
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
                    if (sdPath.back() != '/') sdPath += "/";

                    WIN32_FIND_DATAA fd;
                    HANDLE h = FindFirstFileA((sdPath + "*").c_str(), &fd);
                    if (h == INVALID_HANDLE_VALUE)
                    {
                        _responseBuffer += "550 Directory not found\r\n";
                    }
                    else
                    {
                        FindClose(h);
                        strncpy(cwdName, newPath.c_str(), FTP_CWD_SIZE - 1);
                        _responseBuffer += "250 Directory changed\r\n";
                    }
                }
            }
            else if (strcmp(cmd, "CDUP") == 0)
            {
                if (strcmp(cwdName, "/") == 0)
                {
                    _responseBuffer += "250 Already at root\r\n";
                }
                else
                {
                    char *lastSlash = strrchr(cwdName, '/');
                    if (lastSlash == cwdName)
                        cwdName[1] = '\0';
                    else if (lastSlash)
                        *lastSlash = '\0';
                    _responseBuffer += "250 Directory changed\r\n";
                }
            }
            else if (strcmp(cmd, "MKD") == 0)
            {
                if (!arg)
                {
                    _responseBuffer += "550 No directory name\r\n";
                }
                else
                {
                    std::string sdPath = sd_get_base_path() + std::string(cwdName);
                    if (sdPath.back() != '/') sdPath += "/";
                    sdPath += arg;
                    if (CreateDirectoryA(sdPath.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS)
                    {
                        char msg[512];
                        snprintf(msg, sizeof(msg), "257 \"%s\" created\r\n", arg);
                        _responseBuffer += msg;
                    }
                    else
                        _responseBuffer += "550 Create directory failed\r\n";
                }
            }
            else if (strcmp(cmd, "RMD") == 0)
            {
                if (!arg)
                {
                    _responseBuffer += "550 No directory name\r\n";
                }
                else
                {
                    std::string sdPath = sd_get_base_path() + std::string(cwdName);
                    if (sdPath.back() != '/') sdPath += "/";
                    sdPath += arg;
                    if (RemoveDirectoryA(sdPath.c_str()))
                        _responseBuffer += "250 Directory removed\r\n";
                    else
                        _responseBuffer += "550 Remove directory failed\r\n";
                }
            }
            else if (strcmp(cmd, "DELE") == 0)
            {
                if (!arg)
                {
                    _responseBuffer += "550 No file name\r\n";
                }
                else
                {
                    std::string sdPath = sd_get_base_path() + std::string(cwdName);
                    if (sdPath.back() != '/') sdPath += "/";
                    sdPath += arg;
                    if (DeleteFileA(sdPath.c_str()))
                        _responseBuffer += "250 File deleted\r\n";
                    else
                        _responseBuffer += "550 Delete failed\r\n";
                }
            }
            else if (strcmp(cmd, "SIZE") == 0)
            {
                if (!arg)
                {
                    _responseBuffer += "550 No file name\r\n";
                }
                else
                {
                    std::string sdPath = sd_get_base_path() + std::string(cwdName);
                    if (sdPath.back() != '/') sdPath += "/";
                    sdPath += arg;

                    WIN32_FILE_ATTRIBUTE_DATA attr;
                    if (GetFileAttributesExA(sdPath.c_str(), GetFileExInfoStandard, &attr))
                    {
                        ULONGLONG fileSize = ((ULONGLONG)attr.nFileSizeHigh << 32) | attr.nFileSizeLow;
                        char msg[64];
                        snprintf(msg, sizeof(msg), "213 %llu\r\n", fileSize);
                        _responseBuffer += msg;
                    }
                    else
                        _responseBuffer += "550 File not found\r\n";
                }
            }
            else if (strcmp(cmd, "MDTM") == 0)
            {
                if (!arg)
                {
                    _responseBuffer += "550 No file name\r\n";
                }
                else
                {
                    std::string sdPath = sd_get_base_path() + std::string(cwdName);
                    if (sdPath.back() != '/') sdPath += "/";
                    sdPath += arg;

                    WIN32_FILE_ATTRIBUTE_DATA attr;
                    if (GetFileAttributesExA(sdPath.c_str(), GetFileExInfoStandard, &attr))
                    {
                        SYSTEMTIME st;
                        FileTimeToSystemTime(&attr.ftLastWriteTime, &st);
                        char msg[64];
                        snprintf(msg, sizeof(msg), "213 %04d%02d%02d%02d%02d%02d\r\n",
                                 st.wYear, st.wMonth, st.wDay,
                                 st.wHour, st.wMinute, st.wSecond);
                        _responseBuffer += msg;
                    }
                    else
                        _responseBuffer += "550 File not found\r\n";
                }
            }
            else if (strcmp(cmd, "PASV") == 0)
            {
                // Close previous data connection
                if (data) data.stop();
                dataPassiveConn = false;

                dataServer.stop();
                dataServer.begin(FTP_DATA_PORT_PASV);

                dataPort = FTP_DATA_PORT_PASV;

                char msg[128];
                snprintf(msg, sizeof(msg), "227 Entering Passive Mode (127,0,0,1,%d,%d)\r\n",
                         dataPort >> 8, dataPort & 0xFF);
                _responseBuffer += msg;
                dataPassiveConn = true;
            }
            else if (strcmp(cmd, "LIST") == 0 || strcmp(cmd, "NLST") == 0)
            {
                if (!dataPassiveConn)
                {
                    _responseBuffer += "425 Use PASV first\r\n";
                }
                else
                {
                    // Accept data connection
                    data = std::move(dataServer.available());
                    if (!data)
                    {
                        _responseBuffer += "425 Cannot open data connection\r\n";
                    }
                    else
                    {
                        _responseBuffer += "150 Opening data connection\r\n";
                        // 发送响应后再传数据
                        client.print(_responseBuffer.c_str());
                        _responseBuffer.clear();

                        std::string sdPath = sd_get_base_path();
                        std::string searchPath = sdPath + std::string(cwdName);
                        if (searchPath.back() != '/') searchPath += "/";
                        searchPath += "*";

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

                                const char *perms = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "drwxr-xr-x" : "-rw-r--r--";
                                ULONGLONG fileSize = ((ULONGLONG)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;

                                const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun",
                                                        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
                                char line[512];
                                snprintf(line, sizeof(line), "%s 1 owner group %12llu %s %2d %02d:%02d %s\r\n",
                                         perms, fileSize,
                                         months[st.wMonth - 1], st.wDay,
                                         st.wHour, st.wMinute,
                                         fd.cFileName);
                                data.print(line);
                            } while (FindNextFileA(h, &fd));
                            FindClose(h);
                        }

                        data.stop();
                        dataPassiveConn = false;
                        _responseBuffer += "226 Transfer complete\r\n";
                    }
                }
            }
            else if (strcmp(cmd, "RETR") == 0)
            {
                if (!dataPassiveConn || !arg)
                {
                    _responseBuffer += dataPassiveConn ? "550 No file name\r\n" : "425 Use PASV first\r\n";
                }
                else
                {
                    data = std::move(dataServer.available());
                    if (!data)
                    {
                        _responseBuffer += "425 Cannot open data connection\r\n";
                    }
                    else
                    {
                        std::string sdPath = sd_get_base_path() + std::string(cwdName);
                        if (sdPath.back() != '/') sdPath += "/";
                        sdPath += arg;

                        FILE *f = fopen(sdPath.c_str(), "rb");
                        if (!f)
                        {
                            _responseBuffer += "550 File not found\r\n";
                        }
                        else
                        {
                            _responseBuffer += "150 Opening data connection\r\n";
                            client.print(_responseBuffer.c_str());
                            _responseBuffer.clear();

                            char fileBuf[4096];
                            int n;
                            while ((n = (int)fread(fileBuf, 1, sizeof(fileBuf), f)) > 0)
                                data.write((uint8_t *)fileBuf, n);
                            fclose(f);

                            data.stop();
                            dataPassiveConn = false;
                            _responseBuffer += "226 Transfer complete\r\n";
                        }
                    }
                }
            }
            else if (strcmp(cmd, "STOR") == 0)
            {
                if (!dataPassiveConn || !arg)
                {
                    _responseBuffer += dataPassiveConn ? "550 No file name\r\n" : "425 Use PASV first\r\n";
                }
                else
                {
                    data = std::move(dataServer.available());
                    if (!data)
                    {
                        _responseBuffer += "425 Cannot open data connection\r\n";
                    }
                    else
                    {
                        std::string sdPath = sd_get_base_path() + std::string(cwdName);
                        if (sdPath.back() != '/') sdPath += "/";
                        sdPath += arg;

                        FILE *f = fopen(sdPath.c_str(), "wb");
                        if (!f)
                        {
                            _responseBuffer += "550 Cannot create file\r\n";
                        }
                        else
                        {
                            _responseBuffer += "150 Opening data connection\r\n";
                            client.print(_responseBuffer.c_str());
                            _responseBuffer.clear();

                            char fileBuf[4096];
                            int timeout = 0;
                            while (timeout < 500)
                            {
                                int n = data.read((uint8_t *)fileBuf, sizeof(fileBuf));
                                if (n > 0)
                                {
                                    fwrite(fileBuf, 1, n, f);
                                    timeout = 0;
                                }
                                else if (n == 0)
                                    break;
                                else
                                {
                                    Sleep(10);
                                    timeout++;
                                }
                            }
                            fclose(f);

                            data.stop();
                            dataPassiveConn = false;
                            _responseBuffer += "226 Transfer complete\r\n";
                        }
                    }
                }
            }
            else if (strcmp(cmd, "RNFR") == 0)
            {
                if (!arg)
                {
                    _responseBuffer += "550 No file name\r\n";
                }
                else
                {
                    rnfrCmd = true;
                    strncpy(buf, arg, FTP_BUF_SIZE - 1);
                    _responseBuffer += "350 Ready for RNTO\r\n";
                }
            }
            else if (strcmp(cmd, "RNTO") == 0)
            {
                if (!rnfrCmd || !arg)
                {
                    _responseBuffer += "503 RNFR required first\r\n";
                }
                else
                {
                    std::string sdFrom = sd_get_base_path() + std::string(cwdName);
                    if (sdFrom.back() != '/') sdFrom += "/";
                    sdFrom += buf;

                    std::string sdTo = sd_get_base_path() + std::string(cwdName);
                    if (sdTo.back() != '/') sdTo += "/";
                    sdTo += arg;

                    if (MoveFileA(sdFrom.c_str(), sdTo.c_str()))
                        _responseBuffer += "250 File renamed\r\n";
                    else
                        _responseBuffer += "550 Rename failed\r\n";
                    rnfrCmd = false;
                }
            }
            else if (strcmp(cmd, "QUIT") == 0)
            {
                _responseBuffer += "221 Goodbye\r\n";
                client.print(_responseBuffer.c_str());
                client.stop();
                printf("[FtpServer] Client disconnected\n");
                return;
            }
            else if (strcmp(cmd, "NOOP") == 0)
            {
                _responseBuffer += "200 NOOP ok\r\n";
            }
            else
            {
                char msg[128];
                snprintf(msg, sizeof(msg), "502 '%s': command not understood\r\n", cmd);
                _responseBuffer += msg;
            }

            iCL = 0;
            cmdLine[0] = '\0';
        }
        else if (iCL < FTP_CMD_SIZE - 1)
        {
            cmdLine[iCL++] = c;
        }
    }

    // 发送缓冲的响应
    if (!_responseBuffer.empty())
    {
        client.print(_responseBuffer.c_str());
    }
}