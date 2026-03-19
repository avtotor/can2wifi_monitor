#pragma once

#include <stdint.h>
#include <stdbool.h>

#define GVRET_FRAME_QUEUE_SIZE 64

typedef struct {
    uint32_t timestamp;
    uint32_t id;
    bool extended;
    uint8_t bus;
    uint8_t dlc;
    uint8_t data[8];
} ParsedFrame;

typedef struct {
    uint16_t build_num;
    uint8_t device_type;
    bool valid;
} DeviceInfo;

typedef struct {
    uint32_t speed0;
    bool enabled0;
    bool listen_only0;
    uint32_t speed1;
    bool valid;
} BusParams;

typedef enum {
    GVRET_IDLE,
    GVRET_GET_COMMAND,
    GVRET_READ_CAN_FRAME,
    GVRET_READ_TIME_SYNC,
    GVRET_READ_DEV_INFO,
    GVRET_READ_CANBUS_PARAMS,
    GVRET_READ_KEEPALIVE,
    GVRET_READ_NUMBUSES,
    GVRET_READ_EXT_BUSES,
    GVRET_SKIP_BYTES
} GvretState;

typedef struct {
    GvretState state;
    int step;
    uint8_t buf[32];
    uint8_t current_dlc;
    int skip_remaining;

    ParsedFrame frame_queue[GVRET_FRAME_QUEUE_SIZE];
    int frame_head;
    int frame_tail;
    int frame_count;

    DeviceInfo dev_info;
    BusParams bus_params;
    bool keepalive_flag;
    uint8_t num_buses;
} GvretParser;

void        gvret_parser_init(GvretParser* p);
void        gvret_parser_reset(GvretParser* p);
void        gvret_parser_feed_bytes(GvretParser* p, const uint8_t* data, int len);
bool        gvret_parser_has_frame(const GvretParser* p);
ParsedFrame gvret_parser_pop_frame(GvretParser* p);

int gvret_build_enable_binary(uint8_t* buf);
int gvret_build_get_dev_info(uint8_t* buf);
int gvret_build_get_bus_params(uint8_t* buf);
int gvret_build_keepalive(uint8_t* buf);
int gvret_build_get_num_buses(uint8_t* buf);
int gvret_build_time_sync(uint8_t* buf);
