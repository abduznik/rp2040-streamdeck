# Building on macOS

## 1. Install toolchain (Homebrew)

```bash
brew install cmake arm-none-eabi-gcc python3
```

## 2. Get the Pico SDK

Clone it anywhere outside this project folder and point `PICO_SDK_PATH` at it:

```bash
git clone -b master https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
cd ~/pico-sdk
git submodule update --init
export PICO_SDK_PATH=~/pico-sdk
```

Add the `export PICO_SDK_PATH=~/pico-sdk` line to your `~/.zshrc` so you don't
have to set it every session.

## 3. Set the board type (RP2040 Zero)

The Waveshare RP2040-Zero uses a generic RP2040 target — the stock
`pico_sdk_init()` works fine. If you want onboard NeoPixel/WS2812 support
later (the Zero has one on GPIO 16), that's a separate addition — this
firmware doesn't touch it.

## 4. Build

```bash
cd streamdeck
mkdir build && cd build
cmake -DPICO_BOARD=pico ..
make -j4
```

This produces `streamdeck.uf2` in the `build/` folder.

## 5. Flash

1. Hold the **BOOT** button on the RP2040-Zero, plug it into your Mac via
   USB-C, then release BOOT. It mounts as a mass-storage drive named
   `RPI-RP2`.
2. Drag `streamdeck.uf2` onto that drive (or `cp build/streamdeck.uf2
   /Volumes/RPI-RP2/`).
3. The board reboots automatically running the new firmware, and re-enumerates
   as a composite HID keyboard + serial device.

## 6. Verify it enumerated correctly

```bash
ls /dev/tty.usbmodem*
```

You should see a device node — that's the CDC serial interface the web page
will connect to. The keyboard HID interface needs no special driver; check
**System Settings → Keyboard** to confirm it's recognized, or just open a text
editor and press a button.

## 7. Wiring

Each button: one leg to the GPIO pin, other leg to any GND pin on the board.
Default pin assignment (edit `BUTTON_PINS` in `src/main.c` to change):

| Button | GPIO |
|--------|------|
| 1      | 2    |
| 2      | 3    |
| 3      | 4    |
| 4      | 5    |
| 5      | 6    |
| 6      | 7    |

No external pull-up resistors needed — internal pull-ups are enabled in
firmware.

## 8. Configuring buttons

Open `webconfig/index.html` in **Chrome or Edge** (Safari/Firefox don't
support WebSerial yet — this is the one cross-platform limitation, same on
Windows/Linux). Click **Connect**, select the `usbmodem` port, and you'll see
all 6 buttons with editable actions. Changes apply live — no reboot, no
reflashing. Click **Save to flash** to persist across power cycles.
