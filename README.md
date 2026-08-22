# RP2040 Stream Deck

Open-source USB stream deck firmware for RP2040 boards (RP2040-Zero, etc.). Turn any RP2040 microcontroller into a fully configurable stream deck with a native desktop app.

![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-RP2040-pink)

## Features

- **Composite HID+CDC USB Device** — acts as both a keyboard and a serial port for configuration
- **Desktop App** — native Tauri app for macOS, Windows, and Linux with serial port access
- **Native App Launching** — launch apps directly via OS APIs (no keyboard shortcuts)
- **7 Action Types** — key press, consumer control, macro (true combos), text, paste, app launcher
- **Configurable Pins** — remap any button to any GPIO, any number of buttons
- **Flash Storage** — persist button configs to on-chip flash with CRC32 validation
- **GitHub Pages** — use the webconfig directly from your browser

## Bill of Materials

| Component | Quantity | Notes |
|-----------|----------|-------|
| **RP2040-Zero** | 1 | Any RP2040 board works, but the Zero is compact and has USB-C |
| **MX-compatible switches** | 6 (or your count) | Cherry MX, Gateron, Kailh, etc. — 3-pin or 5-pin |
| **Wire** | ~5-10 cm | 26-28 AWG solid core or stranded, any colors |
| **Keycaps** | 6 (or your count) | MX-compatible keycaps |
| **3D-printed case** | 1 | See [Enclosure](#enclosure) below |
| **USB-C cable** | 1 | For connection and power |

## Enclosure

This project uses the **[OneClick MacroPad](https://makerworld.com/en/models/2593663)** by [zlicer3d](https://makerworld.com/en/@zlicer) on MakerWorld — a clean, customizable 6-button macropad case.

> The original design targets an Arduino ProMicro, but the RP2040-Zero fits perfectly with minor wiring adjustments. Print the case in PLA, no AMS needed.

**Print settings:** PLA, 0.2mm layer height, ~2.3 hours print time.

## Wiring

All buttons are wired **active-low**: one leg to a GPIO pin, the other leg to GND. The firmware enables internal pull-up resistors — **no external resistors needed**.

### Schematic

```
                    RP2040-Zero
                 +--------------+
                 |              |
    Button 1 ----| GPIO 2       |
    Button 2 ----| GPIO 3       |
    Button 3 ----| GPIO 4       |
    Button 4 ----| GPIO 5       |
    Button 5 ----| GPIO 6       |
    Button 6 ----| GPIO 7       |
                 |              |
    All GNDs ----| GND          |
                 |              |
                 |    USB-C ----|---- to computer
                 +--------------+
```

### Per-Button Wiring

Each MX switch has 2 metal pins on the bottom. Wire one pin to GPIO, the other to GND:

```
    MX Switch (bottom view)
    +---------+
    |  +---+  |
    |  |   |  |
    |  +---+  |
    |         |
    o         o
    |         |
    +----+----+
         |
    Pin A ---- GPIO (e.g. GPIO 2)
    Pin B ---- GND (common ground)
```

### Step-by-Step Soldering

1. **Insert switches** into the 3D-printed plate. Make sure the metal pins face **inward** (toward the center of the board).

2. **Solder all GND pins together** — solder a single wire connecting one pin from each switch. This creates a common ground bus. Use a single color (e.g., black) for all ground connections.

3. **Solder individual signal wires** — solder one wire to the remaining free pin of each switch. Use different colors if possible for easy identification:
   - Button 1 -> GPIO 2
   - Button 2 -> GPIO 3
   - Button 3 -> GPIO 4
   - Button 4 -> GPIO 5
   - Button 5 -> GPIO 6
   - Button 6 -> GPIO 7

4. **Connect to RP2040-Zero** — solder the signal wires to the corresponding GPIO pads, and the ground bus to any GND pad on the board.

5. **Test with a multimeter** — before plugging in, check for shorts between GPIO and GND. Each button should show continuity only when pressed.

6. **Close the case** — place the RP2040-Zero inside the case (components facing **away** from the switches for easier identification). Route the USB-C port to the case opening.

### Wiring Diagram (Visual)

```
    +---------------------------------------------------+
    |                   TOP VIEW                        |
    |                                                   |
    |    [SW1]        [SW2]        [SW3]                |
    |      |            |            |                   |
    |      +- GPIO 2    +- GPIO 3    +- GPIO 4          |
    |      |            |            |                   |
    |    [SW4]        [SW5]        [SW6]                |
    |      |            |            |                   |
    |      +- GPIO 5    +- GPIO 6    +- GPIO 7          |
    |      |            |            |                   |
    |      +------------+------------+                   |
    |                    |                               |
    |                   GND                              |
    |                    |                               |
    |              +-----+-----+                         |
    |              | RP2040    |                         |
    |              |  Zero     |                         |
    |              |           |                         |
    |              |  USB-C ---+---- to computer         |
    |              +-----------+                         |
    +---------------------------------------------------+
```

## Quick Start

1. **Edit pin configuration** in `firmware/src/pin_config.h` — set your GPIO pins and number of buttons:
   ```c
   static const uint8_t BUTTON_PINS[] = { 2, 3, 4, 5, 6, 7 };
   #define NUM_BUTTONS (sizeof(BUTTON_PINS) / sizeof(BUTTON_PINS[0]))
   ```

2. **Build the firmware:**
   ```bash
   cd firmware
   mkdir build && cd build
   cmake -DPICO_SDK_PATH=/path/to/pico-sdk ..
   make -j$(nproc)
   ```

3. **Flash** — hold BOOTSEL on the RP2040-Zero, plug in USB, then drag `streamdeck.uf2` onto the `RPI-RP2` drive that appears.

4. **Configure** — open the StreamDeck Config desktop app and connect to your device.

## Desktop App

The native Tauri app provides full device configuration:

```bash
cd app
npm install
npm run dev     # development mode
npm run build   # production build
```

Native apps for macOS (DMG), Windows (EXE), and Linux (AppImage) are available in [Releases](https://github.com/abduznik/rp2040-streamdeck/releases).

### Features
- **Connect** — select your serial port from a dropdown
- **Configure buttons** — 7 action types with full UI
- **App Launcher** — browse installed apps, launch natively via `open -a` / `xdg-open`
- **Macro Recorder** — record key combos by pressing them
- **Save to Flash** — persist mappings across power cycles

## Companion App (Optional)

See [companion/README.md](companion/README.md) for details.

## Customization

### Changing Pins

Edit `firmware/src/pin_config.h`:

```c
static const uint8_t BUTTON_PINS[] = { 2, 3, 4, 5, 6, 7, 10, 11 }; // any GPIO 0-29
```

`NUM_BUTTONS` is automatically calculated — just add or remove pins from the array.

### Action Types

| Action | Description |
|--------|-------------|
| Key | Send a keyboard key press (with modifiers) |
| Consumer | Send a media key (volume, play/pause, etc.) |
| Macro | Record and play back key combos (Ctrl+Shift+V as one press) |
| Text | Type out a string character by character |
| Paste | Copy text to clipboard and send paste shortcut |
| Launcher | Launch an app by name via the companion app |

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

## CI/CD

- **Build**: firmware + desktop apps build on every push/PR
- **Release**: tagged pushes (v*) create a GitHub release with UF2 + DMG + EXE + AppImage
- **Pages**: webconfig auto-deploys to GitHub Pages

## License

MIT License. See [LICENSE](LICENSE) for details.

## Credits

- Enclosure: [OneClick MacroPad](https://makerworld.com/en/models/2593663) by [zlicer3d](https://makerworld.com/en/@zlicer)
- Firmware: RP2040 USB HID using [TinyUSB](https://github.com/hathach/tinyusb) via [Pico SDK](https://github.com/raspberrypi/pico-sdk)
- Desktop app: [Tauri](https://tauri.app/)
