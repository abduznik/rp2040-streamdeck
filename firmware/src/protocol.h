#ifndef PROTOCOL_H_
#define PROTOCOL_H_

#include <stdint.h>
#include <stdbool.h>
#include "mapping_store.h"

// ---------------------------------------------------------------------------
// Wire protocol (binary, over the CDC serial interface)
//
// All packets: [0xAA][CMD][LEN_LO][LEN_HI][...payload...][CHECKSUM]
// CHECKSUM = 8-bit sum of CMD, LEN_LO, LEN_HI, and all payload bytes (mod 256)
//
// Commands:
//   0x01 GET_MAPPING   -> device replies with 0x81 + serialized mapping_table_t
//   0x02 SET_BUTTON    -> payload: [button_index][button_action_t bytes...]
//                         device applies immediately (RAM) and ACKs with 0x82
//   0x03 SAVE_FLASH    -> device persists current RAM table to flash, ACKs 0x83
//   0x04 PING          -> device replies 0x84 with 1-byte firmware version
//
// Replies mirror the same framing: [0xAA][REPLY_CMD][LEN_LO][LEN_HI][payload][CHECKSUM]
// ---------------------------------------------------------------------------

#define PROTO_SOF          0xAA
#define PROTO_MAX_PAYLOAD  4096  // must fit worst-case mapping_table_t

#define CMD_GET_MAPPING    0x01
#define CMD_SET_BUTTON     0x02
#define CMD_SAVE_FLASH     0x03
#define CMD_PING           0x04

#define REPLY_MAPPING      0x81
#define REPLY_BUTTON_ACK   0x82
#define REPLY_SAVE_ACK     0x83
#define REPLY_PONG         0x84
#define REPLY_ERROR        0xFF

#define FW_VERSION         1

// Called repeatedly from the main loop. Reads any available CDC bytes,
// parses complete packets, and dispatches them (updating `table` in place
// and writing replies back out over CDC). Non-blocking.
void protocol_poll(mapping_table_t *table);

#endif
