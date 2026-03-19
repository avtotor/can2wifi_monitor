#pragma once

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    SOCKET sock;
    bool connected;
    char ip[64];
    int port;
    char last_error[128];
} Connection;

bool connection_init_winsock(void);
void connection_cleanup_winsock(void);

void connection_init(Connection* conn);
void connection_deinit(Connection* conn);

bool connection_connect_tcp(Connection* conn, const char* ip, int port, int timeout_ms);
void connection_disconnect(Connection* conn);

int  connection_receive(Connection* conn, uint8_t* buf, int max_len);
bool connection_send(Connection* conn, const uint8_t* buf, int len);

bool connection_discover(char* out_ip, int out_ip_size, int timeout_ms);
