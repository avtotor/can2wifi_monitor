#include "frame_store.h"
#include <cstring>

FrameStore::FrameStore() : total_(0), lastRateCalc_(GetTickCount64()) {}

void FrameStore::update(const ParsedFrame& frame) {
    ULONGLONG now = GetTickCount64();
    total_++;

    auto it = store_.find(frame.id);
    if (it == store_.end()) {
        FrameEntry e;
        e.id = frame.id;
        e.extended = frame.extended;
        e.bus = frame.bus;
        e.dlc = frame.dlc;
        memcpy(e.data, frame.data, 8);
        memset(e.prevData, 0, 8);
        e.count = 1;
        e.countSnapshot = 0;
        e.rate = 0;
        e.firstSeen = now;
        e.lastSeen = now;
        e.dataChanged = true;
        store_[frame.id] = e;
    } else {
        FrameEntry& e = it->second;
        e.dataChanged = (memcmp(e.data, frame.data, 8) != 0);
        if (e.dataChanged)
            memcpy(e.prevData, e.data, 8);
        memcpy(e.data, frame.data, 8);
        e.dlc = frame.dlc;
        e.bus = frame.bus;
        e.count++;
        e.lastSeen = now;
    }
}

void FrameStore::calculateRates() {
    ULONGLONG now = GetTickCount64();
    double elapsed = (now - lastRateCalc_) / 1000.0;
    if (elapsed < 0.5) return;

    for (auto& [id, e] : store_) {
        e.rate = (e.count - e.countSnapshot) / elapsed;
        e.countSnapshot = e.count;
    }
    lastRateCalc_ = now;
}

void FrameStore::clear() {
    store_.clear();
    total_ = 0;
    lastRateCalc_ = GetTickCount64();
}
