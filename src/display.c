#include "display.h"
#include <stdio.h>
#include <string.h>

#define ESC     "\033["
#define RESET   ESC "0m"
#define BOLD    ESC "1m"
#define RED     ESC "31m"
#define GREEN   ESC "32m"
#define YELLOW  ESC "33m"
#define CYAN    ESC "36m"
#define CLR_EOL ESC "K"
#define CLR_EOS ESC "J"
#define HOME    ESC "H"

void display_init(Display* disp) {
    disp->h_out = GetStdHandle(STD_OUTPUT_HANDLE);

    DWORD mode = 0;
    GetConsoleMode(disp->h_out, &mode);
    SetConsoleMode(disp->h_out, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

    printf(ESC "?25l");
    printf(ESC "2J" HOME);
    fflush(stdout);

    disp->start_time = (uint64_t)GetTickCount64();
}

static int get_console_width(Display* disp) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(disp->h_out, &info))
        return info.srWindow.Right - info.srWindow.Left + 1;
    return 80;
}

static int get_console_height(Display* disp) {
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (GetConsoleScreenBufferInfo(disp->h_out, &info))
        return info.srWindow.Bottom - info.srWindow.Top + 1;
    return 25;
}

static const char* format_uptime(uint64_t ms) {
    static char buf[32];
    int sec = (int)(ms / 1000);
    int h = sec / 3600;
    int m = (sec % 3600) / 60;
    int s = sec % 60;
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", h, m, s);
    return buf;
}

void display_render(Display* disp, const Connection* conn, const GvretParser* parser,
                    const FrameStore* store, const char* status_msg) {
    int width  = get_console_width(disp);
    int height = get_console_height(disp);
    if (width < 40) width = 40;

    char line[256], dline[256];
    int llen = width - 1;
    if (llen > 254) llen = 254;
    memset(line,  '-', llen); line[llen]  = '\0';
    memset(dline, '=', llen); dline[llen] = '\0';

    printf(HOME);

    printf(BOLD CYAN " CAN Monitor v1.0" RESET " | ");
    if (conn->connected) {
        printf("Status: " BOLD GREEN "Connected" RESET "  IP: %s:%d",
               conn->ip, conn->port);
    } else {
        printf("Status: " BOLD RED "Disconnected" RESET);
    }
    printf(CLR_EOL "\n");

    if (status_msg && status_msg[0])
        printf(YELLOW " >> %s" RESET, status_msg);
    printf(CLR_EOL "\n");

    const DeviceInfo* dev = &parser->dev_info;
    const BusParams*  bus = &parser->bus_params;
    if (dev->valid)
        printf(" Device: ESP32_RET build %d", dev->build_num);
    else
        printf(" Device: ---");
    if (bus->valid)
        printf("    CAN0: %lu bps%s", (unsigned long)bus->speed0,
               bus->listen_only0 ? " (listen)" : "");
    printf(CLR_EOL "\n");

    uint64_t uptime = (uint64_t)GetTickCount64() - disp->start_time;
    printf(" Total: %llu frames / Unique: %d IDs / Uptime: %s",
           (unsigned long long)store->total, store->count,
           format_uptime(uptime));
    printf(CLR_EOL "\n");

    printf(" %s" CLR_EOL "\n", dline);
    printf(BOLD "  %-20s | DLC | Data                     |  Count  |  /s" RESET CLR_EOL "\n", "CAN ID");
    printf(" %s" CLR_EOL "\n", line);

    int header_lines = 7;
    int max_rows = height - header_lines;
    if (max_rows < 1) max_rows = 1;

    for (int i = 0; i < store->count && i < max_rows; i++) {
        const FrameEntry* e = &store->entries[i];

        char id_str[32];
        if (e->extended)
            snprintf(id_str, sizeof(id_str), "0x%08X", e->id);
        else
            snprintf(id_str, sizeof(id_str), "0x%03X", e->id);

        char data_str[256] = {0};
        int pos = 0;
        for (int j = 0; j < e->dlc && j < 8; j++) {
            bool byte_changed = e->data_changed && (e->data[j] != e->prev_data[j]);
            if (byte_changed)
                pos += snprintf(data_str + pos, sizeof(data_str) - pos,
                                YELLOW "%02X" RESET " ", e->data[j]);
            else
                pos += snprintf(data_str + pos, sizeof(data_str) - pos,
                                "%02X ", e->data[j]);
        }

        int data_vis_len = e->dlc * 3;
        if (data_vis_len > 0) data_vis_len--;

        printf("  %-20s | %d   | %s", id_str, e->dlc, data_str);
        for (int j = data_vis_len; j < 24; j++) putchar(' ');
        printf(" | %7llu | %4.0f", (unsigned long long)e->count, e->rate);
        printf(CLR_EOL "\n");
    }

    printf(CLR_EOS);
    fflush(stdout);
}
