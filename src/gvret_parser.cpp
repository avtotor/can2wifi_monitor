#include "gvret_parser.h"
#include <cstring>

GvretParser::GvretParser()
    : state_(IDLE), step_(0), currentDlc_(0), skipRemaining_(0),
      keepaliveFlag_(false), numBuses_(1) {
    memset(buf_, 0, sizeof(buf_));
}

void GvretParser::feedBytes(const uint8_t* data, int len) {
    for (int i = 0; i < len; i++)
        processByte(data[i]);
}

void GvretParser::reset() {
    state_ = IDLE;
    step_ = 0;
    frames_.clear();
}

ParsedFrame GvretParser::popFrame() {
    ParsedFrame f = frames_.front();
    frames_.erase(frames_.begin());
    return f;
}

void GvretParser::processByte(uint8_t b) {
    switch (state_) {
    case IDLE:
        if (b == 0xF1) {
            state_ = GET_COMMAND;
        }
        break;

    case GET_COMMAND:
        step_ = 0;
        switch (b) {
        case 0x00: state_ = READ_CAN_FRAME; currentDlc_ = 0; break;   // BUILD_CAN_FRAME
        case 0x01: state_ = READ_TIME_SYNC; break;                     // TIME_SYNC (4 bytes)
        case 0x02: skipRemaining_ = 2; state_ = SKIP_BYTES; break;     // DIG_INPUTS
        case 0x03: skipRemaining_ = 15; state_ = SKIP_BYTES; break;    // ANA_INPUTS
        case 0x06: state_ = READ_CANBUS_PARAMS; break;                 // GET_CANBUS_PARAMS (10 bytes)
        case 0x07: state_ = READ_DEV_INFO; break;                      // GET_DEV_INFO (6 bytes)
        case 0x09: state_ = READ_KEEPALIVE; break;                     // KEEPALIVE (2 bytes)
        case 0x0C: state_ = READ_NUMBUSES; break;                      // GET_NUMBUSES (1 byte)
        case 0x0D: state_ = READ_EXT_BUSES; break;                     // GET_EXT_BUSES (15 bytes)
        default:   state_ = IDLE; break;
        }
        break;

    case READ_CAN_FRAME:
        buf_[step_] = b;
        if (step_ == 8) {
            currentDlc_ = b & 0x0F;
            if (currentDlc_ > 8) currentDlc_ = 8;
        }
        if (step_ >= 9 + currentDlc_) {
            // Got checksum byte -> frame complete
            emitCanFrame();
            state_ = IDLE;
        } else {
            step_++;
        }
        break;

    case READ_TIME_SYNC:
        buf_[step_] = b;
        if (step_ >= 3) state_ = IDLE;
        else step_++;
        break;

    case READ_DEV_INFO:
        buf_[step_] = b;
        if (step_ >= 5) {
            devInfo_.buildNum = buf_[0] | ((uint16_t)buf_[1] << 8);
            devInfo_.deviceType = buf_[2];
            devInfo_.valid = true;
            state_ = IDLE;
        } else {
            step_++;
        }
        break;

    case READ_CANBUS_PARAMS:
        buf_[step_] = b;
        if (step_ >= 9) {
            uint8_t flags0 = buf_[0];
            busParams_.enabled0 = (flags0 & 0x01) != 0;
            busParams_.listenOnly0 = (flags0 & 0x10) != 0;
            busParams_.speed0 = buf_[1] | ((uint32_t)buf_[2] << 8) |
                                ((uint32_t)buf_[3] << 16) | ((uint32_t)buf_[4] << 24);
            busParams_.speed1 = buf_[6] | ((uint32_t)buf_[7] << 8) |
                                ((uint32_t)buf_[8] << 16) | ((uint32_t)buf_[9] << 24);
            busParams_.valid = true;
            state_ = IDLE;
        } else {
            step_++;
        }
        break;

    case READ_KEEPALIVE:
        buf_[step_] = b;
        if (step_ >= 1) {
            keepaliveFlag_ = true;
            state_ = IDLE;
        } else {
            step_++;
        }
        break;

    case READ_NUMBUSES:
        numBuses_ = b;
        state_ = IDLE;
        break;

    case READ_EXT_BUSES:
        if (step_ >= 14) state_ = IDLE;
        else step_++;
        break;

    case SKIP_BYTES:
        skipRemaining_--;
        if (skipRemaining_ <= 0) state_ = IDLE;
        break;
    }
}

void GvretParser::emitCanFrame() {
    ParsedFrame f;
    f.timestamp = buf_[0] | ((uint32_t)buf_[1] << 8) |
                  ((uint32_t)buf_[2] << 16) | ((uint32_t)buf_[3] << 24);

    uint32_t rawId = buf_[4] | ((uint32_t)buf_[5] << 8) |
                     ((uint32_t)buf_[6] << 16) | ((uint32_t)buf_[7] << 24);
    f.extended = (rawId & (1u << 31)) != 0;
    f.id = rawId & 0x7FFFFFFF;

    uint8_t lenBus = buf_[8];
    f.dlc = lenBus & 0x0F;
    f.bus = (lenBus >> 4) & 0x0F;

    memset(f.data, 0, 8);
    for (int i = 0; i < f.dlc && i < 8; i++)
        f.data[i] = buf_[9 + i];

    frames_.push_back(f);
}

// --- Command builders ---

int GvretParser::buildEnableBinary(uint8_t* buf) {
    buf[0] = 0xE7;
    return 1;
}

int GvretParser::buildGetDevInfo(uint8_t* buf) {
    buf[0] = 0xF1; buf[1] = 0x07;
    return 2;
}

int GvretParser::buildGetBusParams(uint8_t* buf) {
    buf[0] = 0xF1; buf[1] = 0x06;
    return 2;
}

int GvretParser::buildKeepalive(uint8_t* buf) {
    buf[0] = 0xF1; buf[1] = 0x09;
    return 2;
}

int GvretParser::buildGetNumBuses(uint8_t* buf) {
    buf[0] = 0xF1; buf[1] = 0x0C;
    return 2;
}

int GvretParser::buildTimeSync(uint8_t* buf) {
    buf[0] = 0xF1; buf[1] = 0x01;
    return 2;
}
