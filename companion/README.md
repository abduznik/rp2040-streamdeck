# Stream Deck Companion App

Background service that listens for button presses from a Stream Deck device and launches configured applications.

## Requirements

- Python 3.6+
- pyserial (`pip install pyserial`)

## Installation

```bash
pip install pyserial
```

## Usage

### List available serial ports

```bash
python3 companion.py --list
```

### Auto-detect and run

```bash
python3 companion.py
```

### Specify a port

```bash
python3 companion.py --port /dev/cu.usbmodem1102
```

### Run as daemon

```bash
python3 companion.py --daemon
```

### Run as daemon with specific port

```bash
python3 companion.py --port /dev/cu.usbmodem1102 --daemon
```

## How It Works

1. Connects to the Stream Deck's CDC serial port at 115200 baud
2. Listens for `0xBE [button_index]` notifications when buttons are pressed
3. When a LAUNCHER button is pressed, reads the mapping table via GET_MAPPING protocol
4. Extracts the app name and launches it using the native OS command

## Protocol

- **Serial**: 115200 baud
- **Button press notification**: `[0xBE][button_index]`
- **GET_MAPPING request**: `[0xAA][0x01][0x00][0x00][0x01]`
- **GET_MAPPING response**: `[0xAA][0x81][len_lo][len_hi][payload...][checksum]`

## Supported Actions

| Type | Value | Description |
|------|-------|-------------|
| none | 0 | No action |
| key | 1 | Keyboard key |
| consumer | 2 | Consumer key (media keys) |
| macro | 3 | Macro |
| text | 4 | Text input |
| paste | 5 | Paste text |
| launcher | 6 | Launch application |

## Supported OS

| OS | Value | Launch Command |
|----|-------|----------------|
| macOS | 0 | `open -a <app>` |
| Windows | 1 | `cmd /c start "" <app>` |
| Linux | 2 | `xdg-open <app>` |

## Files

- `companion.py` - Main application
- `companion.pid` - PID file (when running as daemon)
- `companion.log` - Log file (when running as daemon)

## Stopping the Daemon

```bash
kill $(cat companion.pid)
```
