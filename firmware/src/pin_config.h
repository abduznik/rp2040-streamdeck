// ============================================================================
// Pin Configuration — EDIT THIS FILE to match your hardware wiring.
// ============================================================================
// Buttons are wired active-low: one leg to GPIO, other leg to GND.
// Internal pull-up is enabled — no external resistors needed.
//
// Any GPIO 0-29 works on RP2040. GPIO 23-29 have some board-specific
// caveats on certain RP2040 boards — check your board pinout.
//
// The webconfig and companion app will auto-detect how many buttons
// you have configured from the firmware.
// ============================================================================

#ifndef PIN_CONFIG_H_
#define PIN_CONFIG_H_

#include "pico/stdlib.h"

// --- Configure your button pins here ---
// Add, remove, or reorder pins to match YOUR wiring.
// GPIO numbers must be valid RP2040 pins (0-29).

static const uint8_t BUTTON_PINS[] = { 2, 3, 4, 5, 6, 7 };

// Number of buttons (auto-calculated from the array above).
#define NUM_BUTTONS (sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]))

// --- Debounce settings ---
#define DEBOUNCE_MS 15

#endif
