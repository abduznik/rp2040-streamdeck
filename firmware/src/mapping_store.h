#ifndef MAPPING_STORE_H_
#define MAPPING_STORE_H_

#include <stdint.h>
#include <stdbool.h>
#include "pin_config.h"

#define MAX_MACRO_STEPS  16   // a macro action can be a sequence of up to 16 keystrokes
#define TEXT_MAX_LEN     256  // max text for ACTION_TEXT and ACTION_PASTE

typedef enum {
    ACTION_NONE       = 0,
    ACTION_KEY        = 1,   // single keypress (with modifiers)
    ACTION_CONSUMER   = 2,   // consumer control code (media keys, volume, etc.)
    ACTION_MACRO      = 3,   // sequence of keystrokes typed in order
    ACTION_TEXT       = 4,   // types out a literal ASCII string
    ACTION_PASTE      = 5,   // copies text to clipboard (webconfig) + sends paste keystroke
    ACTION_LAUNCHER   = 6,   // companion app launches an OS app by name
} action_type_t;

// Launcher OS targets — determines which native API the companion uses
#define LAUNCHER_MAC    0
#define LAUNCHER_WINDOWS 1
#define LAUNCHER_LINUX  2

typedef struct __attribute__((packed)) {
    uint8_t modifier;   // HID modifier bitmask (ctrl/shift/alt/gui)
    uint8_t keycode;    // HID keycode
} key_step_t;

// NOTE: packed + fixed-size union so the struct has an identical, predictable
// byte layout on the wire — the WebSerial JS side mirrors this exact layout
// when building/parsing SET_BUTTON and GET_MAPPING packets.
typedef struct __attribute__((packed)) {
    uint8_t type;   // action_type_t, stored as a single byte on the wire
    union __attribute__((packed)) {
        key_step_t   key;                         // ACTION_KEY
        uint16_t     consumer_code;               // ACTION_CONSUMER
        struct __attribute__((packed)) {
            key_step_t steps[MAX_MACRO_STEPS];
            uint8_t    count;
        } macro;                                   // ACTION_MACRO
        struct __attribute__((packed)) {
            char text[TEXT_MAX_LEN];
            uint16_t len;                         // supports up to 256 bytes
        } text;                                    // ACTION_TEXT
        struct __attribute__((packed)) {
            char text[TEXT_MAX_LEN];
            uint16_t len;
        } paste;                                   // ACTION_PASTE (same layout as text)
        struct __attribute__((packed)) {
            uint8_t os;                            // LAUNCHER_MAC / LAUNCHER_WINDOWS / LAUNCHER_LINUX
            char text[TEXT_MAX_LEN - 1];           // app name to launch
        } launcher;                                // ACTION_LAUNCHER
    } data;
    char label[24];   // human-readable label shown in the web UI, not used by firmware logic
} button_action_t;

typedef struct __attribute__((packed)) {
    uint32_t magic;                 // validity marker
    uint32_t version;
    button_action_t buttons[NUM_BUTTONS];
    uint32_t crc32;
} mapping_table_t;

// Maximum mapping_table_t size (for worst-case NUM_BUTTONS).
// The flash sector is 4096 bytes; this must fit.
#define MAPPING_TABLE_MAX_SIZE (4 + 4 + (NUM_BUTTONS * sizeof(button_action_t)) + 4)

// Loads mapping from flash into RAM. If flash is blank/invalid, fills in defaults.
void mapping_store_load(mapping_table_t *out);

// Persists the given mapping table to the reserved flash sector.
// Safe to call from the main loop context (disables interrupts internally).
bool mapping_store_save(const mapping_table_t *table);

// Fills a table with sane defaults (F13-F18) so the deck is usable out of the box.
void mapping_store_defaults(mapping_table_t *out);

#endif
