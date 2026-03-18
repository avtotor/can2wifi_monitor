#include "connection.h"
#include "gvret_parser.h"
#include "frame_store.h"
#include "display.h"

#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>

static const char* DEFAULT_IP = "192.168.4.1";
static const int   TELNET_PORT = 23;
static const int   KEEPALIVE_MS = 2000;
static const int   RECONNECT_MS = 3000;
static const int   DISPLAY_INTERVAL_MS = 100;  // 10 Hz refresh

static volatile bool g_quit = false;

static BOOL WINAPI ctrlHandler(DWORD type) {
    (void)type;
    g_quit = true;
    return TRUE;
}

// Returns description of failure, or empty on success
static std::string sendInitCommands(Connection& conn) {
    uint8_t buf[16];
    int len;

    len = GvretParser::buildEnableBinary(buf);
    if (!conn.sendData(buf, len)) return "send 0xE7 failed: " + conn.lastError();
    Sleep(50);

    len = GvretParser::buildGetDevInfo(buf);
    if (!conn.sendData(buf, len)) return "send GetDevInfo failed: " + conn.lastError();

    len = GvretParser::buildGetBusParams(buf);
    if (!conn.sendData(buf, len)) return "send GetBusParams failed: " + conn.lastError();

    len = GvretParser::buildGetNumBuses(buf);
    if (!conn.sendData(buf, len)) return "send GetNumBuses failed: " + conn.lastError();

    return "";
}

int main(int argc, char* argv[]) {
    // Allow IP override from command line
    std::string targetIp = DEFAULT_IP;
    if (argc > 1) targetIp = argv[1];

    if (!Connection::initWinsock()) {
        fprintf(stderr, "Failed to initialize Winsock\n");
        return 1;
    }

    SetConsoleCtrlHandler(ctrlHandler, TRUE);

    Connection conn;
    GvretParser parser;
    FrameStore store;
    Display display;
    display.init();

    ULONGLONG lastKeepalive = 0;
    ULONGLONG lastDisplay = 0;
    ULONGLONG lastConnectAttempt = 0;
    bool discoveryDone = false;
    std::string statusMsg = "Starting...";
    int connectAttempts = 0;

    while (!g_quit) {
        ULONGLONG now = GetTickCount64();

        // --- Connection management ---
        if (!conn.isConnected()) {
            if (now - lastConnectAttempt >= RECONNECT_MS) {
                lastConnectAttempt = now;
                connectAttempts++;

                // Try UDP discovery once
                if (!discoveryDone) {
                    statusMsg = "UDP discovery on port 17222...";
                    display.render(conn, parser, store, statusMsg);
                    std::string foundIp;
                    if (Connection::discover(foundIp, 2000)) {
                        targetIp = foundIp;
                        statusMsg = "Discovered device at " + foundIp;
                    } else {
                        statusMsg = "UDP discovery failed, using default " + targetIp;
                    }
                    discoveryDone = true;
                    display.render(conn, parser, store, statusMsg);
                }

                char msg[128];
                snprintf(msg, sizeof(msg), "TCP connect to %s:%d (attempt #%d)...",
                         targetIp.c_str(), TELNET_PORT, connectAttempts);
                statusMsg = msg;
                display.render(conn, parser, store, statusMsg);

                if (conn.connectTcp(targetIp, TELNET_PORT)) {
                    parser.reset();
                    std::string initErr = sendInitCommands(conn);
                    lastKeepalive = now;
                    statusMsg = initErr.empty() ? "Connected! Sent init commands." : "INIT FAIL: " + initErr;
                    connectAttempts = 0;
                } else {
                    int err = WSAGetLastError();
                    snprintf(msg, sizeof(msg), "TCP connect FAILED (WSA error %d). Retry in %ds...",
                             err, RECONNECT_MS / 1000);
                    statusMsg = msg;
                }
            }
        }

        // --- Receive data ---
        if (conn.isConnected()) {
            uint8_t rxBuf[4096];
            int n = conn.receive(rxBuf, sizeof(rxBuf));
            if (n > 0) {
                parser.feedBytes(rxBuf, n);
            } else if (n < 0) {
                // Connection lost — show reason
                statusMsg = "LOST: " + conn.lastError();
            }
        }

        // --- Process parsed frames ---
        while (parser.hasFrame()) {
            store.update(parser.popFrame());
        }

        // --- Keepalive ---
        if (conn.isConnected() && now - lastKeepalive >= KEEPALIVE_MS) {
            uint8_t buf[4];
            int len = GvretParser::buildKeepalive(buf);
            conn.sendData(buf, len);
            lastKeepalive = now;
        }

        // --- Display update (10 Hz) ---
        if (now - lastDisplay >= DISPLAY_INTERVAL_MS) {
            store.calculateRates();
            display.render(conn, parser, store, statusMsg);
            lastDisplay = now;
        }

        Sleep(1);
    }

    // Cleanup: show cursor, reset colors
    printf("\033[?25h\033[0m\n");
    fflush(stdout);

    conn.disconnect();
    Connection::cleanupWinsock();
    return 0;
}
