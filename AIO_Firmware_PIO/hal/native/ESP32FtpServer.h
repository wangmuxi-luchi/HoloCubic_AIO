#ifndef ESP32_FTP_SERVER_H
#define ESP32_FTP_SERVER_H

#include "NativeServer.h"
#include "FS.h"
#include <string>

class FtpServer
{
    NativeServer _ctrlServer;
    NativeServer _dataServer;
    bool _authenticated;
    bool _dataPassive;
    char _user[64];
    char _pass[64];
    char _cwd[256];
    char _rnfr[256];
    File _transferFile;
    char _recvBuf[512];
    int _recvLen;
    int _dataPort;
    int _lastActivity;
    std::string _responseBuffer;

    void _resp(const char *code, const char *msg);
    void _sendResponse();
    void _processCommand(const char *cmd, const char *arg);
    bool _dataConnect();
    void _closeData();
    void _doList(const char *arg);
    void _doRetrieve(const char *arg);
    void _doStore(const char *arg);

public:
    FtpServer();
    void begin(String uname, String pword);
    void handleFTP();
};

#endif