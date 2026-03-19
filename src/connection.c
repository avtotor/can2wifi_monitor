#include "connection.h"
#include <stdio.h>
#include <string.h>

bool connection_init_winsock(void) {
    WSADATA wsa;
    return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
}

void connection_cleanup_winsock(void) {
    WSACleanup();
}

void connection_init(Connection* conn) {
    conn->sock = INVALID_SOCKET;
    conn->connected = false;
    conn->ip[0] = '\0';
    conn->port = 0;
    conn->last_error[0] = '\0';
}

void connection_deinit(Connection* conn) {
    connection_disconnect(conn);
}

bool connection_connect_tcp(Connection* conn, const char* ip, int port, int timeout_ms) {
    connection_disconnect(conn);

    conn->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (conn->sock == INVALID_SOCKET) return false;

    u_long mode = 1;
    ioctlsocket(conn->sock, FIONBIO, &mode);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    int result = connect(conn->sock, (struct sockaddr*)&addr, sizeof(addr));
    if (result == SOCKET_ERROR) {
        int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK) {
            closesocket(conn->sock);
            conn->sock = INVALID_SOCKET;
            return false;
        }

        fd_set writefds, exceptfds;
        FD_ZERO(&writefds);
        FD_ZERO(&exceptfds);
        FD_SET(conn->sock, &writefds);
        FD_SET(conn->sock, &exceptfds);

        struct timeval tv;
        tv.tv_sec  = timeout_ms / 1000;
        tv.tv_usec = (timeout_ms % 1000) * 1000;

        result = select(0, NULL, &writefds, &exceptfds, &tv);
        if (result <= 0 || FD_ISSET(conn->sock, &exceptfds)) {
            closesocket(conn->sock);
            conn->sock = INVALID_SOCKET;
            return false;
        }

        int opt_val = 0;
        int opt_len = sizeof(opt_val);
        getsockopt(conn->sock, SOL_SOCKET, SO_ERROR, (char*)&opt_val, &opt_len);
        if (opt_val != 0) {
            closesocket(conn->sock);
            conn->sock = INVALID_SOCKET;
            return false;
        }
    }

    conn->connected = true;
    strncpy(conn->ip, ip, sizeof(conn->ip) - 1);
    conn->ip[sizeof(conn->ip) - 1] = '\0';
    conn->port = port;
    return true;
}

void connection_disconnect(Connection* conn) {
    if (conn->sock != INVALID_SOCKET) {
        closesocket(conn->sock);
        conn->sock = INVALID_SOCKET;
    }
    conn->connected = false;
}

int connection_receive(Connection* conn, uint8_t* buf, int max_len) {
    if (!conn->connected) return -1;
    int n = recv(conn->sock, (char*)buf, max_len, 0);
    if (n > 0) return n;
    if (n == 0) {
        snprintf(conn->last_error, sizeof(conn->last_error),
                 "recv: connection closed by ESP32 (n=0)");
        connection_disconnect(conn);
        return -1;
    }
    int err = WSAGetLastError();
    if (err == WSAEWOULDBLOCK) return 0;
    snprintf(conn->last_error, sizeof(conn->last_error), "recv: WSA error %d", err);
    connection_disconnect(conn);
    return -1;
}

bool connection_send(Connection* conn, const uint8_t* buf, int len) {
    if (!conn->connected) return false;
    int sent = send(conn->sock, (const char*)buf, len, 0);
    if (sent == SOCKET_ERROR) {
        int err = WSAGetLastError();
        snprintf(conn->last_error, sizeof(conn->last_error), "send: WSA error %d", err);
        connection_disconnect(conn);
        return false;
    }
    return true;
}

bool connection_discover(char* out_ip, int out_ip_size, int timeout_ms) {
    SOCKET udp = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (udp == INVALID_SOCKET) return false;

    int opt = 1;
    setsockopt(udp, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(bind_addr));
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(17222);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(udp, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) != 0) {
        closesocket(udp);
        return false;
    }

    DWORD timeout_dw = (DWORD)timeout_ms;
    setsockopt(udp, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout_dw, sizeof(timeout_dw));

    uint8_t buf[16];
    struct sockaddr_in from;
    memset(&from, 0, sizeof(from));
    int from_len = sizeof(from);

    int n = recvfrom(udp, (char*)buf, sizeof(buf), 0, (struct sockaddr*)&from, &from_len);
    closesocket(udp);

    if (n == 4 && buf[0] == 0x1C && buf[1] == 0xEF && buf[2] == 0xAC && buf[3] == 0xED) {
        inet_ntop(AF_INET, &from.sin_addr, out_ip, out_ip_size);
        return true;
    }
    return false;
}
