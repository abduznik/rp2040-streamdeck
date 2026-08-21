#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include "pico/stdlib.h"
#include "bsp/board.h"
#include "tusb.h"
#include "usb_descriptors.h"
#include "pin_config.h"
#include "mapping_store.h"
#include "protocol.h"
#include "hardware/flash.h"

// ---------------------------------------------------------------------------
// Debug output via USB CDC (bypasses Pico SDK stdio wrapping).
// Writes directly to CDC interface 0 — readable from the host serial port.
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Button state arrays — sized dynamically from pin_config.h
// ---------------------------------------------------------------------------

static bool     button_state[NUM_BUTTONS];       // debounced state, true = pressed
static bool     button_raw_prev[NUM_BUTTONS];
static uint32_t button_last_change[NUM_BUTTONS];

static mapping_table_t g_mapping;

// ---------------------------------------------------------------------------
// Minimal ASCII -> HID keycode table (covers printable ASCII 0x20-0x7E)
// Modifier bit for shifted characters is returned via *needs_shift.
// ---------------------------------------------------------------------------
static uint8_t ascii_to_hid(char c, bool *needs_shift) {
    *needs_shift = false;
    if (c >= 'a' && c <= 'z') return 0x04 + (c - 'a');
    if (c >= 'A' && c <= 'Z') { *needs_shift = true; return 0x04 + (c - 'A'); }
    if (c >= '1' && c <= '9') return 0x1E + (c - '1');
    if (c == '0') return 0x27;

    switch (c) {
        case ' ': return 0x2C;
        case '\n': return 0x28; // Enter
        case '\t': return 0x2B;
        case '-': return 0x2D;
        case '_': *needs_shift = true; return 0x2D;
        case '=': return 0x2E;
        case '+': *needs_shift = true; return 0x2E;
        case '.': return 0x37;
        case ',': return 0x36;
        case '/': return 0x38;
        case '?': *needs_shift = true; return 0x38;
        case ';': return 0x33;
        case ':': *needs_shift = true; return 0x33;
        default: return 0x00; // unsupported char, sent as a no-op
    }
}

// ---------------------------------------------------------------------------
// HID report senders
// ---------------------------------------------------------------------------

static void send_key(uint8_t modifier, uint8_t keycode) {
    if (!tud_hid_ready()) return;
    uint8_t keycodes[6] = { keycode, 0, 0, 0, 0, 0 };
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycodes);
    board_delay(8);
    tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL); // release
    board_delay(8);
}

static void send_consumer(uint16_t code) {
    bool ready = tud_hid_ready();
    cdc_log("[CONSUMER] code=0x%04X tud_hid_ready=%d\n", (unsigned)code, (int)ready);
    if (!ready) return;
    tud_hid_report(REPORT_ID_CONSUMER, &code, sizeof(code));
    cdc_log("[CONSUMER] report sent\n");
    board_delay(8);
    uint16_t release = 0;
    tud_hid_report(REPORT_ID_CONSUMER, &release, sizeof(release));
    board_delay(8);
    cdc_log("[CONSUMER] release sent\n");
}

static void perform_action(const button_action_t *action, uint8_t btn_idx) {
    cdc_log("[ACTION] type=%d idx=%d\n", (int)action->type, (int)btn_idx);
    switch (action->type) {
        case ACTION_KEY:
            send_key(action->data.key.modifier, action->data.key.keycode);
            break;

        case ACTION_CONSUMER:
            send_consumer(action->data.consumer_code);
            break;

        case ACTION_MACRO: {
            // Macro steps support true key combos:
            //   keycode == 0     → release (send accumulated keys, then all-up)
            //   keycode == 0xFF  → pause (release, 200ms delay)
            //   otherwise        → accumulate modifier + keycode
            //
            // Example: Ctrl+V = 1 step {mod=0x01, key=0x19}, then release step
            // Example: Ctrl+C, Ctrl+V = {Ctrl+C}, release, {Ctrl+V}, release
            uint8_t acc_mod = 0;
            uint8_t acc_keys[6] = {0};
            uint8_t acc_count = 0;

            for (uint8_t i = 0; i < action->data.macro.count; i++) {
                uint8_t mod = action->data.macro.steps[i].modifier;
                uint8_t key = action->data.macro.steps[i].keycode;

                if (key == 0x00) {
                    // Release: flush accumulated combo
                    if (tud_hid_ready()) {
                        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, acc_mod, acc_keys);
                        board_delay(8);
                        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
                        board_delay(8);
                    }
                    acc_mod = 0;
                    acc_count = 0;
                    memset(acc_keys, 0, sizeof(acc_keys));
                } else if (key == 0xFF) {
                    // Pause: release then wait
                    if (tud_hid_ready()) {
                        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, acc_mod, acc_keys);
                        board_delay(8);
                        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
                    }
                    acc_mod = 0;
                    acc_count = 0;
                    memset(acc_keys, 0, sizeof(acc_keys));
                    board_delay(200);
                } else {
                    // Accumulate
                    acc_mod |= mod;
                    if (acc_count < 6) {
                        acc_keys[acc_count++] = key;
                    }
                }
            }
            // Flush any remaining accumulated keys
            if (acc_count > 0 && tud_hid_ready()) {
                tud_hid_keyboard_report(REPORT_ID_KEYBOARD, acc_mod, acc_keys);
                board_delay(8);
                tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
                board_delay(8);
            }
            break;
        }

        case ACTION_TEXT:
            for (uint16_t i = 0; i < action->data.text.len; i++) {
                bool shift;
                uint8_t kc = ascii_to_hid(action->data.text.text[i], &shift);
                if (kc) send_key(shift ? 0x02 /* left shift */ : 0, kc);
            }
            break;

        case ACTION_PASTE:
            // Send paste keystroke: Cmd+V (Mac) or Ctrl+V (Windows/Linux).
            // The webconfig is expected to have written the text to the OS
            // clipboard before the button was pressed.
            // HID keycodes: 0x19 = 'V', 0x08 = GUI(Cmd), 0x01 = Ctrl
            send_key(0x08, 0x19);  // Cmd+V (works on Mac; on Win/Linux the
                                    // webconfig can override by sending a
                                    // SET_BUTTON with different modifiers)
            break;

        case ACTION_LAUNCHER:
            // Notify companion app to launch the app natively.
            // The companion reads the app name via GET_MAPPING and uses
            // OS-native APIs (open -a / xdg-open / ShellExecute).
            {
                uint8_t notify[2] = { 0xBE, btn_idx };
                tud_cdc_write(notify, 2);
                tud_cdc_write_flush();
                cdc_log("[LAUNCHER] btn_idx=%d notified companion\n", (int)btn_idx);
            }
            break;

        case ACTION_NONE:
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Button scanning with simple debounce
// ---------------------------------------------------------------------------

static void buttons_init(void) {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        gpio_init(BUTTON_PINS[i]);
        gpio_set_dir(BUTTON_PINS[i], GPIO_IN);
        gpio_pull_up(BUTTON_PINS[i]);
        button_state[i] = false;
        button_raw_prev[i] = false;
        button_last_change[i] = 0;
    }
}

static void buttons_scan_and_dispatch(void) {
    uint32_t now = board_millis();

    for (int i = 0; i < NUM_BUTTONS; i++) {
        bool raw_pressed = !gpio_get(BUTTON_PINS[i]); // active-low

        if (raw_pressed != button_raw_prev[i]) {
            button_last_change[i] = now;
            button_raw_prev[i] = raw_pressed;
        }

        if ((now - button_last_change[i]) > DEBOUNCE_MS) {
            if (raw_pressed && !button_state[i]) {
                // Rising edge (debounced press)
                button_state[i] = true;
                cdc_log("[BTN] pin=%d idx=%d action_type=%d\n",
                       (int)BUTTON_PINS[i], (int)i,
                       (int)g_mapping.buttons[i].type);

                // For ACTION_PASTE, notify the webconfig first so it can
                // write the text to the OS clipboard before we send Cmd/Ctrl+V.
                if (g_mapping.buttons[i].type == ACTION_PASTE) {
                    uint8_t notify[2] = { 0xBE, (uint8_t)i };
                    tud_cdc_write(notify, 2);
                    tud_cdc_write_flush();
                    board_delay(150); // give webconfig time to write clipboard
                }

                perform_action(&g_mapping.buttons[i], i);
            } else if (!raw_pressed && button_state[i]) {
                button_state[i] = false;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// TinyUSB HID callbacks (required even if unused)
// ---------------------------------------------------------------------------

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type, uint8_t *buffer,
                                uint16_t reqlen) {
    (void) instance; (void) report_id; (void) report_type;
    (void) buffer; (void) reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type, uint8_t const *buffer,
                            uint16_t bufsize) {
    (void) instance; (void) report_id; (void) report_type;
    (void) buffer; (void) bufsize;
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main(void) {
    board_init();
    tusb_init();
    buttons_init();

    mapping_store_load(&g_mapping);

    while (1) {
        tud_task();              // TinyUSB device task — must be called often
        buttons_scan_and_dispatch();
        protocol_poll(&g_mapping); // live reconfig over CDC, applies to RAM immediately

        // One-shot: print boot diagnostics once USB is live
        static bool boot_msg_sent = false;
        if (!boot_msg_sent && tud_cdc_connected()) {
            boot_msg_sent = true;
            cdc_log("[BOOT] === StreamDeck firmware boot ===\n");
            cdc_log("[BOOT] magic @ flash: 0x%08X\n",
                   (unsigned)*(volatile uint32_t *)(XIP_BASE + PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE));
            cdc_log("[BOOT] mapping loaded: btn0 type=%d consumer=0x%04X key=0x%02X mod=0x%02X label=%.24s\n",
                   (int)g_mapping.buttons[0].type,
                   (unsigned)g_mapping.buttons[0].data.consumer_code,
                   (unsigned)g_mapping.buttons[0].data.key.keycode,
                   (unsigned)g_mapping.buttons[0].data.key.modifier,
                   g_mapping.buttons[0].label);
        }
    }

    return 0;
}
