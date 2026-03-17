#pragma once

#include "connection.h"
#include "gvret_parser.h"
#include "frame_store.h"
#include <string>

class Display {
public:
    Display();

    void init();
    void render(const Connection& conn, const GvretParser& parser,
                const FrameStore& store, bool paused,
                const std::string& statusMsg = "");

private:
    HANDLE hOut_;
    ULONGLONG startTime_;

    int getConsoleWidth();
    int getConsoleHeight();
    std::string formatUptime(ULONGLONG ms);
};
