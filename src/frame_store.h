#pragma once

#include "gvret_parser.h"
#include <stdint.h>
#include <stdbool.h>

#define FRAME_STORE_MAX_ENTRIES 512

typedef struct {
    uint32_t id;
    bool extended;
    uint8_t bus;
    uint8_t dlc;
    uint8_t data[8];
    uint8_t prev_data[8];
    uint64_t count;
    uint64_t count_snapshot;
    double rate;
    uint64_t first_seen;
    uint64_t last_seen;
    bool data_changed;
} FrameEntry;

typedef struct {
    FrameEntry entries[FRAME_STORE_MAX_ENTRIES];
    int count;
    uint64_t total;
    uint64_t last_rate_calc;
} FrameStore;

void frame_store_init(FrameStore* store);
void frame_store_update(FrameStore* store, const ParsedFrame* frame);
void frame_store_calculate_rates(FrameStore* store);
void frame_store_clear(FrameStore* store);
