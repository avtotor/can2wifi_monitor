#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#include <string>

class Connection {
public:
    Connection();
    ~Connection();

    static bool initWinsock();
    static void cleanupWinsock();

    bool connectTcp(const std::string& ip, int port, int timeoutMs = 2000);
    void disconnect();
    bool isConnected() const { return connected_; }

    // Non-blocking receive. Returns bytes read, 0 if no data, -1 on error/disconnect.
    int receive(uint8_t* buf, int maxLen);
    bool sendData(const uint8_t* buf, int len);

    const std::string& getIp() const { return ip_; }
    int getPort() const { return port_; }
    const std::string& lastError() const { return lastError_; }

    // Listen for ESP32 UDP broadcast on port 17222, return source IP.
    static bool discover(std::string& outIp, int timeoutMs = 3000);

private:
    SOCKET sock_;
    bool connected_;
    std::string ip_;
    int port_;
    std::string lastError_;
};
