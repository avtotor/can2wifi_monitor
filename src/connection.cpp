#include "connection.h"

Connection::Connection() : sock_(INVALID_SOCKET), connected_(false), port_(0) {}

Connection::~Connection() { disconnect(); }

bool Connection::initWinsock() {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

void Connection::cleanupWinsock() { WSACleanup(); }

bool Connection::connectTcp(const std::string& ip, int port, int timeoutMs) {
    disconnect();

    sock_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock_ == INVALID_SOCKET) return false;

    // Set non-blocking for connect with timeout
    u_long mode = 1;
    ioctlsocket(sock_, FIONBIO, &mode);

    struct sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

    int result = ::connect(sock_, (struct sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
            return false;
        }

        // Wait for connection
        fd_set writefds, exceptfds;
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        FD_SET(sock_, &writefds);
        FD_SET(sock_, &exceptfds);
        struct timeval tv;
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;

        result = select(0, nullptr, &writefds, &exceptfds, &tv);
        if (result <= 0 || FD_ISSET(sock_, &exceptfds)) {
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
            return false;
        }

        // Verify no socket error
        int optVal = 0;
        int optLen = sizeof(optVal);
        getsockopt(sock_, SOL_SOCKET, SO_ERROR, (char*)&optVal, &optLen);
        if (optVal != 0) {
            closesocket(sock_);
            sock_ = INVALID_SOCKET;
            return false;
        }
    }

    // Socket stays non-blocking
    connected_ = true;
    ip_ = ip;
    port_ = port;
    return true;
}

void Connection::disconnect() {
    if (sock_ != INVALID_SOCKET) {
        closesocket(sock_);
        sock_ = INVALID_SOCKET;
    }
    connected_ = false;
}

int Connection::receive(uint8_t* buf, int maxLen) {
    if (!connected_) return -1;
    int n = recv(sock_, (char*)buf, maxLen, 0);
    if (n > 0) return n;
    if (n == 0) {
        lastError_ = "recv: connection closed by ESP32 (n=0)";
        disconnect();
        return -1;
    }
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) return 0;
    char msg[64];
    snprintf(msg, sizeof(msg), "recv: WSA error %d", err);
    lastError_ = msg;
    disconnect();
    return -1;
}

bool Connection::sendData(const uint8_t* buf, int len) {
    if (!connected_) return false;
    int sent = send(sock_, (const char*)buf, len, 0);
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        char msg[64];
        snprintf(msg, sizeof(msg), "send: WSA error %d", err);
        lastError_ = msg;
        disconnect();
        return false;
    }
    return true;
}

bool Connection::discover(std::string& outIp, int timeoutMs) {
    SOCKET udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp == INVALID_SOCKET) return false;

    int opt = 1;
    setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in bindAddr = {};
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(17222);
    bindAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp, (struct sockaddr*)&bindAddr, sizeof(bindAddr)) != 0) {
        closesocket(udp);
        return false;
    }

    DWORD timeout = (DWORD)timeoutMs;
    setsockopt(udp, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    uint8_t buf[16];
    struct sockaddr_in from = {};
    int fromLen = sizeof(from);

    int n = recvfrom(udp, (char*)buf, sizeof(buf), 0, (struct sockaddr*)&from, &fromLen);
    closesocket(udp);

    if (n == 4 && buf[0] == 0x1C && buf[1] == 0xEF && buf[2] == 0xAC && buf[3] == 0xED) {
        char ipStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from.sin_addr, ipStr, sizeof(ipStr));
        outIp = ipStr;
        return true;
    }

    return false;
}
