#include "connection.h"
#include "gvret_parser.h"
#include "frame_store.h"
#include "display.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const int TELNET_PORT         = 23;
static const int KEEPALIVE_MS        = 2000;
static const int RECONNECT_MS        = 3000;
static const int DISPLAY_INTERVAL_MS = 100;

static volatile BOOL g_quit = FALSE;

static BOOL WINAPI ctrl_handler(DWORD type) {
    (void)type;
    g_quit = TRUE;
    return TRUE;
}

static bool send_init_commands(Connection* conn, char* err_buf, int err_size) {
    uint8_t buf[16];
    int len;

    len = gvret_build_enable_binary(buf);
    if (!connection_send(conn, buf, len)) {
        snprintf(err_buf, err_size, "send 0xE7 failed: %s", conn->last_error);
        return false;
    }
    Sleep(50);

    len = gvret_build_get_dev_info(buf);
    if (!connection_send(conn, buf, len)) {
        snprintf(err_buf, err_size, "send GetDevInfo failed: %s", conn->last_error);
        return false;
    }

    len = gvret_build_get_bus_params(buf);
    if (!connection_send(conn, buf, len)) {
        snprintf(err_buf, err_size, "send GetBusParams failed: %s", conn->last_error);
        return false;
    }

    len = gvret_build_get_num_buses(buf);
    if (!connection_send(conn, buf, len)) {
        snprintf(err_buf, err_size, "send GetNumBuses failed: %s", conn->last_error);
        return false;
    }

    return true;
}

int main(int argc, char* argv[]) {
    char target_ip[64] = {0};
    bool skip_discovery = false;

    if (argc > 1) {
        strncpy(target_ip, argv[1], sizeof(target_ip) - 1);
        skip_discovery = true;
    }

    if (!connection_init_winsock()) {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return 1;
    }

    SetConsoleCtrlHandler(ctrl_handler, TRUE);

    Connection  conn;
    GvretParser parser;
    FrameStore  store;
    Display     disp;

    connection_init(&conn);
    gvret_parser_init(&parser);
    frame_store_init(&store);
    display_init(&disp);

    ULONGLONG last_keepalive       = 0;
    ULONGLONG last_display         = 0;
    ULONGLONG last_connect_attempt = 0;
    bool discovery_done = skip_discovery;
    char status_msg[256] = "Starting...";
    int connect_attempts = 0;

    while (!g_quit) {
        ULONGLONG now = GetTickCount64();

        // --- Connection management ---
        if (!conn.connected) {
            if (now - last_connect_attempt >= (ULONGLONG)RECONNECT_MS) {
                last_connect_attempt = now;

                // UDP discovery if no IP was given on command line
                if (!discovery_done) {
                    connect_attempts++;
                    snprintf(status_msg, sizeof(status_msg),
                             "UDP discovery on port 17222... (attempt #%d)", connect_attempts);
                    display_render(&disp, &conn, &parser, &store, status_msg);

                    char found_ip[64] = {0};
                    if (connection_discover(found_ip, sizeof(found_ip), 5000)) {
                        snprintf(target_ip, sizeof(target_ip), "%s", found_ip);
                        snprintf(status_msg, sizeof(status_msg),
                                 "Discovered device at %s", found_ip);
                        discovery_done = true;
                        connect_attempts = 0;
                    } else {
                        snprintf(status_msg, sizeof(status_msg),
                                 "No device found (attempt #%d). Provide IP: %s <address>",
                                 connect_attempts, argv[0]);
                        display_render(&disp, &conn, &parser, &store, status_msg);
                        continue;
                    }
                    display_render(&disp, &conn, &parser, &store, status_msg);
                }

                connect_attempts++;
                snprintf(status_msg, sizeof(status_msg),
                         "TCP connect to %s:%d (attempt #%d)...",
                         target_ip, TELNET_PORT, connect_attempts);
                display_render(&disp, &conn, &parser, &store, status_msg);

                if (connection_connect_tcp(&conn, target_ip, TELNET_PORT, 2000)) {
                    gvret_parser_reset(&parser);
                    char init_err[256] = {0};
                    bool ok = send_init_commands(&conn, init_err, sizeof(init_err));
                    last_keepalive = now;
                    if (ok)
                        snprintf(status_msg, sizeof(status_msg), "Connected! Sent init commands.");
                    else
                        snprintf(status_msg, sizeof(status_msg), "INIT FAIL: %s", init_err);
                    connect_attempts = 0;
                } else {
                    int err = WSAGetLastError();
                    snprintf(status_msg, sizeof(status_msg),
                             "TCP connect FAILED (WSA error %d). Retry in %ds...",
                             err, RECONNECT_MS / 1000);
                }
            }
        }

        // --- Receive data ---
        if (conn.connected) {
            uint8_t rx_buf[4096];
            int n = connection_receive(&conn, rx_buf, sizeof(rx_buf));
            if (n > 0) {
                gvret_parser_feed_bytes(&parser, rx_buf, n);
            } else if (n < 0) {
                snprintf(status_msg, sizeof(status_msg), "LOST: %s", conn.last_error);
            }
        }

        // --- Process parsed frames ---
        while (gvret_parser_has_frame(&parser)) {
            ParsedFrame f = gvret_parser_pop_frame(&parser);
            frame_store_update(&store, &f);
        }

        // --- Keepalive ---
        if (conn.connected && now - last_keepalive >= (ULONGLONG)KEEPALIVE_MS) {
            uint8_t buf[4];
            int len = gvret_build_keepalive(buf);
            connection_send(&conn, buf, len);
            last_keepalive = now;
        }

        // --- Display update (10 Hz) ---
        if (now - last_display >= (ULONGLONG)DISPLAY_INTERVAL_MS) {
            frame_store_calculate_rates(&store);
            display_render(&disp, &conn, &parser, &store, status_msg);
            last_display = now;
        }

        Sleep(1);
    }

    printf("\033[?25h\033[0m\n");
    fflush(stdout);

    connection_disconnect(&conn);
    connection_cleanup_winsock();
    return 0;
}
