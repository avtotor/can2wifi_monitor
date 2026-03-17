#include "display.h"
#include <cstdio>
#include <cstring>

// ANSI escape helpers
#define ESC "\033["
#define RESET   ESC "0m"
#define BOLD    ESC "1m"
#define RED     ESC "31m"
#define GREEN   ESC "32m"
#define YELLOW  ESC "33m"
#define CYAN    ESC "36m"
#define CLR_EOL ESC "K"
#define CLR_EOS ESC "J"
#define HOME    ESC "H"

Display::Display() : hOut_(INVALID_HANDLE_VALUE), startTime_(0) {}

void Display::init() {
    hOut_ = GetStdHandle(STD_OUTPUT_HANDLE);

    // Enable VT100 processing for ANSI escape codes
    DWORD mode = 0;
    GetConsoleMode(hOut_, &mode);
    SetConsoleMode(hOut_, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    // Hide cursor
    printf(ESC "?25l");

    // Clear screen
    printf(ESC "2J" HOME);
    fflush(stdout);

    startTime_ = GetTickCount64();
}

int Display::getConsoleWidth() {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(hOut_, &info))
        return info.srWindow.Right - info.srWindow.Left + 1;
    return 80;
}

int Display::getConsoleHeight() {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(hOut_, &info))
        return info.srWindow.Bottom - info.srWindow.Top + 1;
    return 25;
}

std::string Display::formatUptime(ULONGLONG ms) {
    int sec = (int)(ms / 1000);
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    return buf;
}

void Display::render(const Connection& conn, const GvretParser& parser,
                     const FrameStore& store, bool paused,
                     const std::string& statusMsg) {
    int width = getConsoleWidth();
    int height = getConsoleHeight();
    if (width < 40) width = 40;

    std::string line(width - 1, '-');
    std::string dline(width - 1, '=');

    // Move cursor home
    printf(HOME);

    // Title
    printf(BOLD CYAN " CAN Monitor v1.0" RESET);
    if (paused) printf(BOLD YELLOW "  [PAUSED]" RESET);
    printf(CLR_EOL "\n");

    // Separator
    printf(" %s" CLR_EOL "\n", line.c_str());

    // Connection status
    if (conn.isConnected()) {
        printf(" Status: " BOLD GREEN "Connected" RESET "    IP: %s:%d",
               conn.getIp().c_str(), conn.getPort());
    } else {
        printf(" Status: " BOLD RED "Disconnected" RESET "                   ");
    }
    printf(CLR_EOL "\n");

    // Diagnostic line
    if (!statusMsg.empty()) {
        printf(" >> " YELLOW "%s" RESET, statusMsg.c_str());
    }
    printf(CLR_EOL "\n");

    // Device info
    const auto& dev = parser.deviceInfo();
    const auto& bus = parser.busParams();
    if (dev.valid) {
        printf(" Device: ESP32_RET build %d", dev.buildNum);
    } else {
        printf(" Device: ---");
    }
    if (bus.valid) {
        printf("    CAN0: %lu bps%s", (unsigned long)bus.speed0,
               bus.listenOnly0 ? " (listen)" : "");
    }
    printf(CLR_EOL "\n");

    // Stats
    ULONGLONG uptime = GetTickCount64() - startTime_;
    printf(" Total: %llu frames   Unique: %zu IDs   Uptime: %s",
           (unsigned long long)store.totalFrames(), store.uniqueIds(),
           formatUptime(uptime).c_str());
    printf(CLR_EOL "\n");

    // Double separator
    printf(" %s" CLR_EOL "\n", dline.c_str());

    // Column headers
    printf(BOLD "  CAN ID     | DLC | Data                     |  Count  |  /s" RESET CLR_EOL "\n");
    printf(" %s" CLR_EOL "\n", line.c_str());

    // Frame rows
    int headerLines = 9;  // lines used above (including diagnostic line)
    int footerLines = 2;  // separator + hotkeys
    int maxRows = height - headerLines - footerLines;
    if (maxRows < 1) maxRows = 1;

    int row = 0;
    for (const auto& [id, e] : store.entries()) {
        if (row >= maxRows) break;

        // Format CAN ID
        char idStr[16];
        if (e.extended)
            snprintf(idStr, sizeof(idStr), "0x%08X", id);
        else
            snprintf(idStr, sizeof(idStr), "0x%03X    ", id);

        // Format data bytes, highlight changed bytes
        char dataStr[80] = {};
        int pos = 0;
        for (int i = 0; i < e.dlc && i < 8; i++) {
            bool byteChanged = e.dataChanged && (e.data[i] != e.prevData[i]);
            if (byteChanged)
                pos += snprintf(dataStr + pos, sizeof(dataStr) - pos, YELLOW "%02X" RESET " ", e.data[i]);
            else
                pos += snprintf(dataStr + pos, sizeof(dataStr) - pos, "%02X ", e.data[i]);
        }

        // Visible width of data (without ANSI codes) for padding
        int dataVisLen = e.dlc * 3;
        if (dataVisLen > 0) dataVisLen--; // trailing space

        // Print row - we need to pad data column manually due to ANSI codes
        printf("  %s | %d   | %s", idStr, e.dlc, dataStr);
        // Pad data column to 24 chars visible
        for (int i = dataVisLen; i < 24; i++) putchar(' ');
        printf(" | %7llu | %4.0f", (unsigned long long)e.count, e.rate);
        printf(CLR_EOL "\n");
        row++;
    }

    // Clear remaining rows
    printf(CLR_EOS);

    // Move to bottom for hotkeys
    // Use absolute positioning: go to line (height-1)
    printf(ESC "%d;1H", height - 1);
    printf(" %s" CLR_EOL "\n", line.c_str());
    printf(" [Q]uit  [P]ause  [C]lear  [R]econnect" CLR_EOL);

    fflush(stdout);
}
