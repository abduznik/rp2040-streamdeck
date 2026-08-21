#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "mapping_store.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
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

// Reserve the LAST sector of flash for our mapping table.
// FLASH_SECTOR_SIZE is 4096 bytes on RP2040 — plenty for 6 button_action_t entries.
#define FLASH_TARGET_OFFSET (PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE)
#define MAGIC_VALUE  0x53444B31  // "SDK1"
#define TABLE_VERSION 2

static const uint8_t *flash_target_contents = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET);

// Small CRC32 (poly 0xEDB88320) — good enough for corruption detection, not security.
static uint32_t crc32_calc(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int32_t)(crc & 1)));
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

// Compile-time check: mapping table must fit in one flash sector.
_Static_assert(sizeof(mapping_table_t) <= FLASH_SECTOR_SIZE,
               "mapping_table_t exceeds 4KB flash sector — reduce NUM_BUTTONS or struct size");

void mapping_store_defaults(mapping_table_t *out) {
    memset(out, 0, sizeof(mapping_table_t));
    out->magic = MAGIC_VALUE;
    out->version = TABLE_VERSION;

    // Default: each button sends F13, F14, F15... cycling through F13-F24.
    // These keys almost never collide with anything, so it's safe out of the box.
    static const uint8_t f_keycodes[] = {
        0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, // F13-F18
        0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, // F19-F24 (reuse, HID doesn't care)
    };
    const int num_fkeys = sizeof(f_keycodes) / sizeof(f_keycodes[0]);

    for (int i = 0; i < NUM_BUTTONS; i++) {
        out->buttons[i].type = ACTION_KEY;
        out->buttons[i].data.key.modifier = 0;
        out->buttons[i].data.key.keycode = f_keycodes[i % num_fkeys];
        int fnum = 13 + (i % 12);
        snprintf(out->buttons[i].label, sizeof(out->buttons[i].label), "F%d", fnum);
    }
}

void mapping_store_load(mapping_table_t *out) {
    const mapping_table_t *stored = (const mapping_table_t *) flash_target_contents;

    cdc_log("[LOAD] flash @ %p: magic=0x%08X ver=%d crc_stored=0x%08X\n",
           (const void *)stored,
           (unsigned)stored->magic, (int)stored->version,
           (unsigned)stored->crc32);

    if (stored->magic == MAGIC_VALUE && stored->version == TABLE_VERSION) {
        uint32_t computed = crc32_calc((const uint8_t *)stored,
                                        sizeof(mapping_table_t) - sizeof(uint32_t));
        cdc_log("[LOAD] crc_computed=0x%08X match=%d\n",
               (unsigned)computed, (int)(computed == stored->crc32));
        if (computed == stored->crc32) {
            memcpy(out, stored, sizeof(mapping_table_t));
            cdc_log("[LOAD] loaded from flash OK\n");
            return;
        }
        cdc_log("[LOAD] CRC MISMATCH — falling back to defaults\n");
    } else {
        cdc_log("[LOAD] magic/version invalid — falling back to defaults\n");
    }

    // Blank or corrupt flash — start from defaults. Caller may choose to
    // save these back immediately so next boot loads cleanly.
    mapping_store_defaults(out);
}

bool mapping_store_save(const mapping_table_t *table_in) {
    // Copy so we can fix up magic/version/crc without mutating caller's copy
    mapping_table_t table;
    memcpy(&table, table_in, sizeof(mapping_table_t));
    table.magic = MAGIC_VALUE;
    table.version = TABLE_VERSION;
    table.crc32 = crc32_calc((const uint8_t *)&table, sizeof(mapping_table_t) - sizeof(uint32_t));

    cdc_log("[SAVE] pre-write: crc_computed=0x%08X btn0_type=%d btn0_consumer=0x%04X\n",
           (unsigned)table.crc32, (int)table.buttons[0].type,
           (unsigned)table.buttons[0].data.consumer_code);

    // Flash writes must be a multiple of FLASH_PAGE_SIZE (256B) and erases a
    // multiple of FLASH_SECTOR_SIZE (4096B). We pad our struct into one page-
    // aligned buffer sized to the sector so a single erase+program covers it.
    static uint8_t buf[FLASH_SECTOR_SIZE];
    memset(buf, 0xFF, sizeof(buf));
    memcpy(buf, &table, sizeof(mapping_table_t));

    uint32_t irq_state = save_and_disable_interrupts();
    cdc_log("[SAVE] interrupts disabled, erasing sector...\n");
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    cdc_log("[SAVE] erase done, programming...\n");
    flash_range_program(FLASH_TARGET_OFFSET, buf, FLASH_SECTOR_SIZE);
    cdc_log("[SAVE] program done, restoring interrupts\n");
    restore_interrupts(irq_state);

    // Verify
    const mapping_table_t *check = (const mapping_table_t *) flash_target_contents;
    bool ok = check->magic == MAGIC_VALUE && check->crc32 == table.crc32;
    cdc_log("[SAVE] verify: flash_magic=0x%08X flash_crc=0x%08X expected_crc=0x%08X result=%d\n",
           (unsigned)check->magic, (unsigned)check->crc32,
           (unsigned)table.crc32, (int)ok);
    return ok;
}
