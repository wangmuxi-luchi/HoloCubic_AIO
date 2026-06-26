/*
 * Native WiFi bridge for Windows
 *
 * Provides real TCP/IP networking through Win32 sockets
 * Compatible with ESP32 WiFi API
 */

#include "network_bridge.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>

#pragma comment(lib, "Ws2_32.lib")

static bool g_wsa_initialized = false;

void network_bridge_init()
{
    if (!g_wsa_initialized)
    {
        WSADATA wsa_data;
        WSAStartup(MAKEWORD(2, 2), &wsa_data);
        g_wsa_initialized = true;
        printf("[network_bridge] WSA initialized\n");
    }
}

// WiFi implementation
WiFiClass WiFi;

bool WiFiClass::begin(const char *ssid, const char *password)
{
    printf("[WiFi] Connecting to %s...\n", ssid);
    printf("[WiFi] Native simulation: Already connected to host network\n");
    return true;
}

bool WiFiClass::connected()
{
    return true;
}

IPAddress WiFiClass::localIP()
{
    return IPAddress(127, 0, 0, 1);
}

WiFiClient::WiFiClient() : _socket(INVALID_SOCKET), _connected(false)
{
}

WiFiClient::WiFiClient(SOCKET sock) : _socket(sock), _connected(sock != INVALID_SOCKET)
{
}

WiFiClient::~WiFiClient()
{
    stop();
}

WiFiClient::WiFiClient(WiFiClient &&other) noexcept
    : _socket(other._socket), _connected(other._connected)
{
    other._socket = INVALID_SOCKET;
    other._connected = false;
}

WiFiClient &WiFiClient::operator=(WiFiClient &&other) noexcept
{
    if (this != &other)
    {
        stop();
        _socket = other._socket;
        _connected = other._connected;
        other._socket = INVALID_SOCKET;
        other._connected = false;
    }
    return *this;
}

int WiFiClient::connect(const char *host, uint16_t port)
{
    network_bridge_init();
    // Resolve hostname
    addrinfo hints, *result;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%d", port);

    if (getaddrinfo(host, portStr, &hints, &result) != 0)
        return 0;

    SOCKET sock = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (sock == INVALID_SOCKET)
    {
        freeaddrinfo(result);
        return 0;
    }

    if (::connect(sock, result->ai_addr, (int)result->ai_addrlen) != 0)
    {
        closesocket(sock);
        freeaddrinfo(result);
        return 0;
    }

    freeaddrinfo(result);
    _socket = sock;
    _connected = true;
    return 1;
}

int WiFiClient::connect(IPAddress ip, uint16_t port)
{
    network_bridge_init();
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET)
        return 0;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(ip);

    if (::connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        closesocket(sock);
        return 0;
    }

    _socket = sock;
    _connected = true;
    return 1;
}

void WiFiClient::stop()
{
    if (_socket != INVALID_SOCKET)
    {
        closesocket(_socket);
        _socket = INVALID_SOCKET;
    }
    _connected = false;
}

int WiFiClient::available()
{
    if (!_connected)
        return 0;
    u_long available = 0;
    if (ioctlsocket(_socket, FIONREAD, &available) == 0)
        return (int)available;
    return 0;
}

int WiFiClient::read()
{
    if (!_connected)
        return -1;
    uint8_t b;
    int n = recv(_socket, (char *)&b, 1, 0);
    if (n == 0)
    {
        _connected = false;
        return 0;
    }
    if (n < 0)
    {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return -1;
        _connected = false;
        return -1;
    }
    return (int)b;
}

int WiFiClient::read(uint8_t *buf, size_t size)
{
    if (!_connected)
        return -1;
    int n = recv(_socket, (char *)buf, (int)size, 0);
    if (n == 0)
    {
        _connected = false;
        return 0;
    }
    if (n < 0)
    {
        if (WSAGetLastError() == WSAEWOULDBLOCK)
            return -1;
        _connected = false;
        return -1;
    }
    return n;
}

size_t WiFiClient::write(uint8_t b)
{
    return write(&b, 1);
}

size_t WiFiClient::write(const uint8_t *buf, size_t size)
{
    if (!_connected)
        return 0;
    int sent = send(_socket, (const char *)buf, (int)size, 0);
    if (sent < 0)
        return 0;
    return (size_t)sent;
}

size_t WiFiClient::print(const char *str)
{
    size_t len = strlen(str);
    return write((const uint8_t *)str, len);
}

size_t WiFiClient::println(const char *str)
{
    size_t n = print(str);
    n += print("\r\n");
    return n;
}

size_t WiFiClient::println()
{
    return print("\r\n");
}

bool WiFiClient::connected() const
{
    return _connected;
}

bool WiFiClient::operator!=(const WiFiClient &other) const
{
    return _socket != other._socket;
}

WiFiServer::WiFiServer(uint16_t port) : _port(port), _listenSocket(INVALID_SOCKET)
{
}

WiFiServer::~WiFiServer()
{
    stop();
}

void WiFiServer::begin()
{
    network_bridge_init();
    if (_listenSocket != INVALID_SOCKET)
        closesocket(_listenSocket);

    _listenSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (_listenSocket == INVALID_SOCKET)
    {
        printf("[WiFiServer] socket() failed\n");
        return;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(_listenSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(_port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(_listenSocket, (struct sockaddr *)&addr, sizeof(addr)) != 0)
    {
        int err = WSAGetLastError();
        printf("[WiFiServer] bind() failed (port %d), err %d\n", _port, err);
        closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;
        return;
    }

    if (listen(_listenSocket, 5) != 0)
    {
        printf("[WiFiServer] listen() failed\n");
        closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;
        return;
    }

    // Set non-blocking
    u_long mode = 1;
    ioctlsocket(_listenSocket, FIONBIO, &mode);

    printf("[WiFiServer] Listening on port %d\n", _port);
}

WiFiClient WiFiServer::available()
{
    if (_listenSocket == INVALID_SOCKET)
        return WiFiClient();
    SOCKET client = accept(_listenSocket, NULL, NULL);
    if (client == INVALID_SOCKET)
    {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK)
            printf("[WiFiServer] accept() err %d\n", err);
        return WiFiClient();
    }
    // Set non-blocking
    u_long mode = 1;
    ioctlsocket(client, FIONBIO, &mode);
    return WiFiClient(client);
}

bool WiFiServer::hasClient()
{
    if (_listenSocket == INVALID_SOCKET)
        return false;
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(_listenSocket, &fds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    int ret = select((int)_listenSocket + 1, &fds, NULL, NULL, &tv);
    return ret > 0 && FD_ISSET(_listenSocket, &fds);
}

void WiFiServer::stop()
{
    if (_listenSocket != INVALID_SOCKET)
    {
        closesocket(_listenSocket);
        _listenSocket = INVALID_SOCKET;
    }
}