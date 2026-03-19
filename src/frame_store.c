#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "frame_store.h"
#include <string.h>

void frame_store_init(FrameStore* store) {
    memset(store, 0, sizeof(*store));
    store->last_rate_calc = (uint64_t)GetTickCount64();
}

static int find_entry(const FrameStore* store, uint32_t id) {
    int lo = 0, hi = store->count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (store->entries[mid].id == id) return mid;
        if (store->entries[mid].id < id) lo = mid + 1;
        else hi = mid - 1;
    }
    return -1;
}

static int insert_entry(FrameStore* store, uint32_t id) {
    if (store->count >= FRAME_STORE_MAX_ENTRIES) return -1;

    int lo = 0, hi = store->count;
    while (lo < hi) {
        int mid = (lo + hi) / 2;
        if (store->entries[mid].id < id) lo = mid + 1;
        else hi = mid;
    }
    int pos = lo;

    for (int i = store->count; i > pos; i--)
        store->entries[i] = store->entries[i - 1];

    memset(&store->entries[pos], 0, sizeof(FrameEntry));
    store->entries[pos].id = id;
    store->count++;
    return pos;
}

void frame_store_update(FrameStore* store, const ParsedFrame* frame) {
    uint64_t now = (uint64_t)GetTickCount64();
    store->total++;

    int idx = find_entry(store, frame->id);
    if (idx < 0) {
        idx = insert_entry(store, frame->id);
        if (idx < 0) return;
        FrameEntry* e = &store->entries[idx];
        e->extended    = frame->extended;
        e->bus         = frame->bus;
        e->dlc         = frame->dlc;
        memcpy(e->data, frame->data, 8);
        e->count       = 1;
        e->first_seen  = now;
        e->last_seen   = now;
        e->data_changed = true;
    } else {
        FrameEntry* e = &store->entries[idx];
        e->data_changed = (memcmp(e->data, frame->data, 8) != 0);
        if (e->data_changed)
            memcpy(e->prev_data, e->data, 8);
        memcpy(e->data, frame->data, 8);
        e->dlc      = frame->dlc;
        e->bus      = frame->bus;
        e->count++;
        e->last_seen = now;
    }
}

void frame_store_calculate_rates(FrameStore* store) {
    uint64_t now = (uint64_t)GetTickCount64();
    double elapsed = (now - store->last_rate_calc) / 1000.0;
    if (elapsed < 0.5) return;

    for (int i = 0; i < store->count; i++) {
        FrameEntry* e = &store->entries[i];
        e->rate = (double)(e->count - e->count_snapshot) / elapsed;
        e->count_snapshot = e->count;
    }
    store->last_rate_calc = now;
}

void frame_store_clear(FrameStore* store) {
    store->count = 0;
    store->total = 0;
    store->last_rate_calc = (uint64_t)GetTickCount64();
}
