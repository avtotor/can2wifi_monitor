#pragma once

#include "connection.h"
#include "gvret_parser.h"
#include "frame_store.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

typedef struct {
    HANDLE h_out;
    uint64_t start_time;
} Display;

void display_init(Display* disp);
void display_render(Display* disp, const Connection* conn, const GvretParser* parser,
                    const FrameStore* store, const char* status_msg);
