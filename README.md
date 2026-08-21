# RP2040 Stream Deck

Open-source USB stream deck firmware for RP2040 boards (RP2040-Zero, etc.). Turn any RP2040 microcontroller into a fully configurable stream deck with web-based configuration and a companion app.

## Features

- **Composite HID+CDC USB Device** — acts as both a keyboard and a serial port for configuration
- **Webconfig UI** — configure buttons from your browser via Chrome/Edge WebSerial
- **Companion App** — Python-based CLI tool for configuration and firmware updates
- **6 Action Types** — key press, consumer control, macro, text output, paste, and app launcher
- **Configurable Pins** — remap any button to any GPIO in `pin_config.h`
- **Flash Storage** — persist button configs to on-chip flash
- **Auto-adjusting NUM_BUTTONS** — automatically recalculates based on pin definitions

## Hardware

### Wiring

Connect each button between a GPIO pin and GND. The firmware enables the internal pull-up resistor, so no external resistors are needed.

```
GPIO pin ──┬── Button ── GND
           │
        (internal pull-up)
```

### Supported Boards

- RP2040-Zero
- XIAO RP2040
- RP2040-Pico (with pin remapping)
- Any RP2040 board with available GPIOs

## Quick Start

1. **Edit pin configuration** in `firmware/src/pin_config.h` — set your GPIO pins and number of buttons.
2. **Build the firmware:**
   ```bash
   cd firmware
   mkdir build && cd build
   cmake -DPICO_SDK_PATH=/path/to/pico-sdk ..
   make -j$(nproc)
   ```
3. **Flash** — hold BOOTSEL, plug in USB, then drag `streamdeck.uf2` onto the RPI-RP2 drive.

## Webconfig

1. Open `webconfig/index.html` in Chrome or Edge (WebSerial required).
2. Connect to the stream deck's serial port.
3. Configure button actions, then save to flash.

## Companion App

```bash
pip install pyserial
cd companion
python companion.py
```

The companion app provides a command-line interface for configuring buttons and managing firmware.

## Customization

### Changing Pins

Edit `firmware/src/pin_config.h`:

```c
#define PIN_0   0
#define PIN_1   1
#define PIN_2   2
// ... add as many pins as you need
#define NUM_BUTTONS 6
```

`NUM_BUTTONS` is automatically calculated from the pin definitions, so you only need to add or remove pin entries.

### Adding Actions

Each button can be assigned one of six action types:

| Action    | Description                          |
|-----------|--------------------------------------|
| `key`     | Send a keyboard key press            |
| `consumer`| Send a consumer control (media key)  |
| `macro`   | Send a sequence of keys              |
| `text`    | Type a string                        |
| `paste`   | Paste from clipboard                 |
| `launcher`| Launch an application                |

## Build Instructions

### Prerequisites

- [Pico SDK](https://github.com/raspberrypi/pico-sdk) installed
- CMake 3.13+
- ARM GCC toolchain

### Build

```bash
cd firmware
mkdir build && cd build
cmake -DPICO_SDK_PATH=/path/to/pico-sdk ..
make -j$(nproc)
```

Output: `firmware/build/streamdeck.uf2`

## License

MIT License. See [LICENSE](LICENSE) for details.
