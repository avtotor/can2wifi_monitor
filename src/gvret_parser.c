#include "gvret_parser.h"
#include <string.h>

void gvret_parser_init(GvretParser* p) {
    memset(p, 0, sizeof(*p));
    p->state = GVRET_IDLE;
    p->num_buses = 1;
}

void gvret_parser_reset(GvretParser* p) {
    p->state = GVRET_IDLE;
    p->step = 0;
    p->frame_head = 0;
    p->frame_tail = 0;
    p->frame_count = 0;
}

bool gvret_parser_has_frame(const GvretParser* p) {
    return p->frame_count > 0;
}

ParsedFrame gvret_parser_pop_frame(GvretParser* p) {
    ParsedFrame f = p->frame_queue[p->frame_head];
    p->frame_head = (p->frame_head + 1) % GVRET_FRAME_QUEUE_SIZE;
    p->frame_count--;
    return f;
}

static void emit_can_frame(GvretParser* p) {
    if (p->frame_count >= GVRET_FRAME_QUEUE_SIZE) return;

    ParsedFrame f;
    memset(&f, 0, sizeof(f));

    f.timestamp = p->buf[0] | ((uint32_t)p->buf[1] << 8) |
                  ((uint32_t)p->buf[2] << 16) | ((uint32_t)p->buf[3] << 24);

    uint32_t raw_id = p->buf[4] | ((uint32_t)p->buf[5] << 8) |
                      ((uint32_t)p->buf[6] << 16) | ((uint32_t)p->buf[7] << 24);
    f.extended = (raw_id & (1u << 31)) != 0;
    f.id = raw_id & 0x7FFFFFFF;

    uint8_t len_bus = p->buf[8];
    f.dlc = len_bus & 0x0F;
    f.bus = (len_bus >> 4) & 0x0F;

    int dlc = f.dlc < 8 ? f.dlc : 8;
    for (int i = 0; i < dlc; i++)
        f.data[i] = p->buf[9 + i];

    p->frame_queue[p->frame_tail] = f;
    p->frame_tail = (p->frame_tail + 1) % GVRET_FRAME_QUEUE_SIZE;
    p->frame_count++;
}

static void process_byte(GvretParser* p, uint8_t b) {
    switch (p->state) {
    case GVRET_IDLE:
        if (b == 0xF1)
            p->state = GVRET_GET_COMMAND;
        break;

    case GVRET_GET_COMMAND:
        p->step = 0;
        switch (b) {
        case 0x00: p->state = GVRET_READ_CAN_FRAME;    p->current_dlc = 0; break;
        case 0x01: p->state = GVRET_READ_TIME_SYNC;    break;
        case 0x02: p->skip_remaining = 2;  p->state = GVRET_SKIP_BYTES; break;
        case 0x03: p->skip_remaining = 15; p->state = GVRET_SKIP_BYTES; break;
        case 0x06: p->state = GVRET_READ_CANBUS_PARAMS; break;
        case 0x07: p->state = GVRET_READ_DEV_INFO;     break;
        case 0x09: p->state = GVRET_READ_KEEPALIVE;    break;
        case 0x0C: p->state = GVRET_READ_NUMBUSES;     break;
        case 0x0D: p->state = GVRET_READ_EXT_BUSES;    break;
        default:   p->state = GVRET_IDLE;               break;
        }
        break;

    case GVRET_READ_CAN_FRAME:
        p->buf[p->step] = b;
        if (p->step == 8) {
            p->current_dlc = b & 0x0F;
            if (p->current_dlc > 8) p->current_dlc = 8;
        }
        if (p->step >= 9 + p->current_dlc) {
            emit_can_frame(p);
            p->state = GVRET_IDLE;
        } else {
            p->step++;
        }
        break;

    case GVRET_READ_TIME_SYNC:
        p->buf[p->step] = b;
        if (p->step >= 3) p->state = GVRET_IDLE;
        else p->step++;
        break;

    case GVRET_READ_DEV_INFO:
        p->buf[p->step] = b;
        if (p->step >= 5) {
            p->dev_info.build_num   = p->buf[0] | ((uint16_t)p->buf[1] << 8);
            p->dev_info.device_type = p->buf[2];
            p->dev_info.valid = true;
            p->state = GVRET_IDLE;
        } else {
            p->step++;
        }
        break;

    case GVRET_READ_CANBUS_PARAMS:
        p->buf[p->step] = b;
        if (p->step >= 9) {
            uint8_t flags0 = p->buf[0];
            p->bus_params.enabled0     = (flags0 & 0x01) != 0;
            p->bus_params.listen_only0 = (flags0 & 0x10) != 0;
            p->bus_params.speed0 = p->buf[1] | ((uint32_t)p->buf[2] << 8) |
                                   ((uint32_t)p->buf[3] << 16) | ((uint32_t)p->buf[4] << 24);
            p->bus_params.speed1 = p->buf[6] | ((uint32_t)p->buf[7] << 8) |
                                   ((uint32_t)p->buf[8] << 16) | ((uint32_t)p->buf[9] << 24);
            p->bus_params.valid = true;
            p->state = GVRET_IDLE;
        } else {
            p->step++;
        }
        break;

    case GVRET_READ_KEEPALIVE:
        p->buf[p->step] = b;
        if (p->step >= 1) {
            p->keepalive_flag = true;
            p->state = GVRET_IDLE;
        } else {
            p->step++;
        }
        break;

    case GVRET_READ_NUMBUSES:
        p->num_buses = b;
        p->state = GVRET_IDLE;
        break;

    case GVRET_READ_EXT_BUSES:
        if (p->step >= 14) p->state = GVRET_IDLE;
        else p->step++;
        break;

    case GVRET_SKIP_BYTES:
        p->skip_remaining--;
        if (p->skip_remaining <= 0) p->state = GVRET_IDLE;
        break;
    }
}

void gvret_parser_feed_bytes(GvretParser* p, const uint8_t* data, int len) {
    for (int i = 0; i < len; i++)
        process_byte(p, data[i]);
}

int gvret_build_enable_binary(uint8_t* buf) { buf[0] = 0xE7; return 1; }
int gvret_build_get_dev_info(uint8_t* buf)  { buf[0] = 0xF1; buf[1] = 0x07; return 2; }
int gvret_build_get_bus_params(uint8_t* buf){ buf[0] = 0xF1; buf[1] = 0x06; return 2; }
int gvret_build_keepalive(uint8_t* buf)     { buf[0] = 0xF1; buf[1] = 0x09; return 2; }
int gvret_build_get_num_buses(uint8_t* buf) { buf[0] = 0xF1; buf[1] = 0x0C; return 2; }
int gvret_build_time_sync(uint8_t* buf)     { buf[0] = 0xF1; buf[1] = 0x01; return 2; }
