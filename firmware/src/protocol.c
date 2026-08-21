#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "protocol.h"
#include "tusb.h"

static void cdc_log(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (len <= 0) return;
    if (len > (int)sizeof(buf)) len = (int)sizeof(buf);
    tud_cdc_write(buf, len);
    tud_cdc_write_flush();
}

typedef enum {
    WAIT_SOF,
    WAIT_CMD,
    WAIT_LEN_LO,
    WAIT_LEN_HI,
    WAIT_PAYLOAD,
    WAIT_CHECKSUM
} parse_state_t;

static parse_state_t state = WAIT_SOF;
static uint8_t  cmd;
static uint16_t payload_len;
static uint16_t payload_idx;
static uint8_t  payload[PROTO_MAX_PAYLOAD];
static uint8_t  running_checksum;

// tud_cdc_write() only queues as many bytes as fit in the CDC TX FIFO
// (CFG_TUD_CDC_TX_BUFSIZE = 256) and returns the count actually queued, so
// for payloads larger than that (e.g. the ~552-byte mapping table) we must
// loop, flushing and giving the USB stack a chance to drain in between.
static void cdc_write_all(const uint8_t *data, uint16_t len) {
    uint16_t sent = 0;
    while (sent < len) {
        uint32_t n = tud_cdc_write(data + sent, len - sent);
        sent += n;
        tud_cdc_write_flush();
        if (sent < len) tud_task(); // let TinyUSB service the endpoint and free FIFO space
    }
}

static void send_packet(uint8_t reply_cmd, const uint8_t *data, uint16_t len) {
    uint8_t header[4] = {
        PROTO_SOF, reply_cmd, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)
    };
    uint8_t checksum = reply_cmd + header[2] + header[3];
    for (uint16_t i = 0; i < len; i++) checksum += data[i];

    cdc_write_all(header, sizeof(header));
    if (len) cdc_write_all(data, len);
    cdc_write_all(&checksum, 1);
    tud_cdc_write_flush();
}

static void handle_packet(mapping_table_t *table) {
    cdc_log("[PROTO] cmd=0x%02X len=%d\n", (int)cmd, (int)payload_len);
    switch (cmd) {
        case CMD_GET_MAPPING:
            cdc_log("[PROTO] GET_MAPPING -> sending %d bytes\n", (int)sizeof(mapping_table_t));
            send_packet(REPLY_MAPPING, (const uint8_t *) table, sizeof(mapping_table_t));
            break;

        case CMD_SET_BUTTON: {
            if (payload_len != 1 + sizeof(button_action_t)) {
                cdc_log("[PROTO] SET_BUTTON bad len %d\n", (int)payload_len);
                send_packet(REPLY_ERROR, NULL, 0);
                break;
            }
            uint8_t idx = payload[0];
            if (idx >= NUM_BUTTONS) {
                cdc_log("[PROTO] SET_BUTTON bad idx %d\n", (int)idx);
                send_packet(REPLY_ERROR, NULL, 0);
                break;
            }
            memcpy(&table->buttons[idx], &payload[1], sizeof(button_action_t));
            cdc_log("[PROTO] SET_BUTTON idx=%d type=%d\n",
                   (int)idx, (int)table->buttons[idx].type);
            send_packet(REPLY_BUTTON_ACK, &idx, 1);
            break;
        }

        case CMD_SAVE_FLASH: {
            cdc_log("[PROTO] SAVE_FLASH — calling mapping_store_save()\n");
            bool ok = mapping_store_save(table);
            uint8_t result = ok ? 1 : 0;
            cdc_log("[PROTO] SAVE_FLASH result=%d\n", (int)ok);
            send_packet(REPLY_SAVE_ACK, &result, 1);
            break;
        }

        case CMD_PING: {
            uint8_t ver = FW_VERSION;
            send_packet(REPLY_PONG, &ver, 1);
            break;
        }

        default:
            send_packet(REPLY_ERROR, NULL, 0);
            break;
    }
}

void protocol_poll(mapping_table_t *table) {
    if (!tud_cdc_connected()) return;

    while (tud_cdc_available()) {
        uint8_t byte;
        if (tud_cdc_read(&byte, 1) != 1) break;

        switch (state) {
            case WAIT_SOF:
                if (byte == PROTO_SOF) state = WAIT_CMD;
                break;

            case WAIT_CMD:
                cmd = byte;
                running_checksum = byte;
                state = WAIT_LEN_LO;
                break;

            case WAIT_LEN_LO:
                payload_len = byte;
                running_checksum += byte;
                state = WAIT_LEN_HI;
                break;

            case WAIT_LEN_HI:
                payload_len |= ((uint16_t) byte) << 8;
                running_checksum += byte;
                payload_idx = 0;
                if (payload_len > PROTO_MAX_PAYLOAD) {
                    // Malformed / too large — bail back to hunting for SOF
                    state = WAIT_SOF;
                } else if (payload_len == 0) {
                    state = WAIT_CHECKSUM;
                } else {
                    state = WAIT_PAYLOAD;
                }
                break;

            case WAIT_PAYLOAD:
                payload[payload_idx++] = byte;
                running_checksum += byte;
                if (payload_idx >= payload_len) state = WAIT_CHECKSUM;
                break;

            case WAIT_CHECKSUM:
                if (byte == running_checksum) {
                    handle_packet(table);
                } else {
                    cdc_log("[PROTO] checksum mismatch: expected 0x%02X got 0x%02X\n",
                           (int)running_checksum, (int)byte);
                    send_packet(REPLY_ERROR, NULL, 0);
                }
                state = WAIT_SOF;
                break;
        }
    }
}
