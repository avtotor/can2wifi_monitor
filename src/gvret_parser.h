#pragma once

#include <cstdint>
#include <vector>

struct ParsedFrame {
    uint32_t timestamp;
    uint32_t id;
    bool extended;
    uint8_t bus;
    uint8_t dlc;
    uint8_t data[8];
};

struct DeviceInfo {
    uint16_t buildNum = 0;
    uint8_t deviceType = 0;
    bool valid = false;
};

struct BusParams {
    uint32_t speed0 = 0;
    bool enabled0 = false;
    bool listenOnly0 = false;
    uint32_t speed1 = 0;
    bool valid = false;
};

class GvretParser {
public:
    GvretParser();

    void feedBytes(const uint8_t* data, int len);
    void reset();

    bool hasFrame() const { return !frames_.empty(); }
    ParsedFrame popFrame();

    const DeviceInfo& deviceInfo() const { return devInfo_; }
    const BusParams& busParams() const { return busParams_; }
    bool gotKeepalive() const { return keepaliveFlag_; }
    void clearKeepalive() { keepaliveFlag_ = false; }
    uint8_t numBuses() const { return numBuses_; }

    // Build commands to send to ESP32
    static int buildEnableBinary(uint8_t* buf);  // returns length
    static int buildGetDevInfo(uint8_t* buf);
    static int buildGetBusParams(uint8_t* buf);
    static int buildKeepalive(uint8_t* buf);
    static int buildGetNumBuses(uint8_t* buf);
    static int buildTimeSync(uint8_t* buf);

private:
    enum State {
        IDLE, GET_COMMAND, READ_CAN_FRAME,
        READ_TIME_SYNC, READ_DEV_INFO, READ_CANBUS_PARAMS,
        READ_KEEPALIVE, READ_NUMBUSES, READ_EXT_BUSES,
        SKIP_BYTES
    };

    State state_;
    int step_;
    uint8_t buf_[32];
    uint8_t currentDlc_;
    int skipRemaining_;

    std::vector<ParsedFrame> frames_;
    DeviceInfo devInfo_;
    BusParams busParams_;
    bool keepaliveFlag_;
    uint8_t numBuses_;

    void processByte(uint8_t b);
    void emitCanFrame();
};
