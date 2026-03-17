#pragma once

#include "gvret_parser.h"
#include <windows.h>
#include <cstdint>
#include <map>

struct FrameEntry {
    uint32_t id = 0;
    bool extended = false;
    uint8_t bus = 0;
    uint8_t dlc = 0;
    uint8_t data[8] = {};
    uint8_t prevData[8] = {};
    uint64_t count = 0;
    uint64_t countSnapshot = 0;
    double rate = 0.0;
    ULONGLONG firstSeen = 0;
    ULONGLONG lastSeen = 0;
    bool dataChanged = false;
};

class FrameStore {
public:
    FrameStore();

    void update(const ParsedFrame& frame);
    void calculateRates();
    void clear();

    uint64_t totalFrames() const { return total_; }
    size_t uniqueIds() const { return store_.size(); }
    const std::map<uint32_t, FrameEntry>& entries() const { return store_; }

private:
    std::map<uint32_t, FrameEntry> store_;
    uint64_t total_ = 0;
    ULONGLONG lastRateCalc_ = 0;
};
