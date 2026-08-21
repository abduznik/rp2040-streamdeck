#!/usr/bin/env python3
"""
Stream Deck Companion App

Background service that listens for button presses from the Stream Deck
device and launches configured applications.
"""

import argparse
import glob
import os
import signal
import struct
import subprocess
import sys
import time
from datetime import datetime

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial is required. Install with: pip install pyserial")
    sys.exit(1)

# Protocol constants
SOF = 0xAA
CMD_GET_MAPPING = 0x01
CMD_NOTIFY_BUTTON = 0xBE

# Button action types
ACTION_NONE = 0
ACTION_KEY = 1
ACTION_CONSUMER = 2
ACTION_MACRO = 3
ACTION_TEXT = 4
ACTION_PASTE = 5
ACTION_LAUNCHER = 6

# OS types for launcher
OS_MAC = 0
OS_WIN = 1
OS_LINUX = 2

# Mapping table layout
BUTTON_ACTION_SIZE = 283
NUM_BUTTONS = 6
MAPPING_HEADER_SIZE = 8  # magic(4) + version(4)
MAPPING_BUTTONS_SIZE = NUM_BUTTONS * BUTTON_ACTION_SIZE  # 1698
MAPPING_CRC_SIZE = 4
MAPPING_TOTAL_SIZE = MAPPING_HEADER_SIZE + MAPPING_BUTTONS_SIZE + MAPPING_CRC_SIZE  # 1710


class StreamDeckCompanion:
    def __init__(self, port=None, daemon=False, list_ports=False):
        self.port = port
        self.daemon = daemon
        self.list_ports = list_ports
        self.serial_conn = None
        self.running = False
        self.pid_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "companion.pid")
        self.log_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "companion.log")

    def log(self, message):
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        line = f"[{timestamp}] {message}"
        print(line, flush=True)
        if self.daemon:
            try:
                with open(self.log_file, "a") as f:
                    f.write(line + "\n")
            except IOError:
                pass

    def list_available_ports(self):
        ports = serial.tools.list_ports.comports()
        usbmodem_ports = []
        for p in ports:
            if "usbmodem" in p.device or "usbmodem" in p.description.lower():
                usbmodem_ports.append(p)
        return usbmodem_ports if usbmodem_ports else ports

    def auto_detect_port(self):
        ports = self.list_available_ports()
        if not ports:
            self.log("No serial ports found")
            return None
        if len(ports) == 1:
            self.log(f"Auto-detected port: {ports[0].device}")
            return ports[0].device
        self.log("Multiple serial ports found:")
        for i, p in enumerate(ports):
            self.log(f"  [{i}] {p.device} - {p.description}")
        self.log("Use --port to specify which port to use")
        return ports[0].device

    def connect(self):
        if not self.port:
            self.port = self.auto_detect_port()
        if not self.port:
            self.log("Error: No serial port available")
            return False
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=115200,
                timeout=1,
                write_timeout=1
            )
            self.log(f"Connected to {self.port}")
            return True
        except serial.SerialException as e:
            self.log(f"Error connecting to {self.port}: {e}")
            return False

    def disconnect(self):
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            self.log("Disconnected")

    def calculate_checksum(self, data):
        return sum(data) & 0xFF

    def build_get_mapping_request(self):
        cmd = CMD_GET_MAPPING
        length = 0x0001
        payload = [cmd, length & 0xFF, (length >> 8) & 0xFF]
        checksum = self.calculate_checksum(payload)
        return bytes([SOF] + payload + [checksum])

    def send_get_mapping(self):
        request = self.build_get_mapping_request()
        self.log(f"Sending GET_MAPPING: {request.hex()}")
        self.serial_conn.write(request)
        self.serial_conn.flush()
        return self.read_response()

    def read_response(self, timeout=2.0):
        self.serial_conn.timeout = timeout
        start_time = time.time()
        while time.time() - start_time < timeout:
            byte = self.serial_conn.read(1)
            if not byte:
                continue
            if byte[0] == SOF:
                cmd_byte = self.serial_conn.read(1)
                if not cmd_byte:
                    continue
                if cmd_byte[0] == (CMD_GET_MAPPING | 0x80):
                    length_bytes = self.serial_conn.read(2)
                    if len(length_bytes) < 2:
                        continue
                    length = length_bytes[0] | (length_bytes[1] << 8)
                    payload = self.serial_conn.read(length)
                    if len(payload) < length:
                        continue
                    checksum = self.serial_conn.read(1)
                    if not checksum:
                        continue
                    expected_checksum = self.calculate_checksum([cmd_byte[0]] + list(length_bytes) + list(payload))
                    if checksum[0] != expected_checksum:
                        self.log(f"Checksum mismatch: expected {expected_checksum:#x}, got {checksum[0]:#x}")
                        continue
                    return payload
        return None

    def parse_mapping_table(self, data):
        if len(data) < MAPPING_TOTAL_SIZE:
            self.log(f"Response too short: {len(data)} bytes (expected {MAPPING_TOTAL_SIZE})")
            return None
        mapping = {}
        mapping["magic"] = struct.unpack_from("<I", data, 0)[0]
        mapping["version"] = struct.unpack_from("<I", data, 4)[0]
        mapping["buttons"] = []
        for i in range(NUM_BUTTONS):
            offset = MAPPING_HEADER_SIZE + i * BUTTON_ACTION_SIZE
            button_data = data[offset:offset + BUTTON_ACTION_SIZE]
            button = self.parse_button_action(button_data)
            mapping["buttons"].append(button)
        mapping["crc32"] = struct.unpack_from("<I", data, MAPPING_HEADER_SIZE + MAPPING_BUTTONS_SIZE)[0]
        return mapping

    def parse_button_action(self, data):
        button = {}
        button["type"] = data[0]
        button["union"] = data[1:259]
        button["label"] = data[259:283].decode("ascii", errors="replace").rstrip("\x00")
        if button["type"] == ACTION_LAUNCHER:
            os_type = button["union"][0]
            app_name_bytes = button["union"][1:258]
            app_name = app_name_bytes.decode("utf-8", errors="replace").rstrip("\x00")
            button["os"] = os_type
            button["app_name"] = app_name
        return button

    def launch_app(self, app_name, os_type):
        if not app_name:
            self.log("Error: No app name configured for this button")
            return False
        self.log(f"Launching: {app_name} (OS type: {os_type})")
        try:
            if os_type == OS_MAC:
                subprocess.run(["open", "-a", app_name], check=False)
            elif os_type == OS_WIN:
                subprocess.run(["cmd", "/c", "start", "", app_name], check=False)
            elif os_type == OS_LINUX:
                subprocess.run(["xdg-open", app_name], check=False)
            else:
                self.log(f"Error: Unknown OS type {os_type}")
                return False
            self.log(f"Successfully launched {app_name}")
            return True
        except Exception as e:
            self.log(f"Error launching {app_name}: {e}")
            return False

    def handle_button_press(self, button_index):
        self.log(f"Button {button_index} pressed")
        response = self.send_get_mapping()
        if not response:
            self.log("Error: No response to GET_MAPPING")
            return
        mapping = self.parse_mapping_table(response)
        if not mapping:
            self.log("Error: Failed to parse mapping table")
            return
        if button_index >= NUM_BUTTONS:
            self.log(f"Error: Button index {button_index} out of range (max {NUM_BUTTONS - 1})")
            return
        button = mapping["buttons"][button_index]
        if button["type"] == ACTION_LAUNCHER:
            self.launch_app(button["app_name"], button["os"])
        else:
            action_types = {ACTION_NONE: "none", ACTION_KEY: "key", ACTION_CONSUMER: "consumer",
                          ACTION_MACRO: "macro", ACTION_TEXT: "text", ACTION_PASTE: "paste"}
            self.log(f"Button {button_index} is not a launcher (type: {action_types.get(button['type'], 'unknown')})")

    def health_check(self):
        self.log("Health check: pinging device...")
        response = self.send_get_mapping()
        if response:
            self.log("Health check: device is alive")
            return True
        self.log("Health check: no response from device")
        return False

    def listen_loop(self):
        self.serial_conn.timeout = 1
        last_health_check = time.time()
        health_check_interval = 30.0
        while self.running:
            byte = self.serial_conn.read(1)
            if not byte:
                current_time = time.time()
                if current_time - last_health_check >= health_check_interval:
                    self.health_check()
                    last_health_check = current_time
                continue
            if byte[0] == CMD_NOTIFY_BUTTON:
                idx_byte = self.serial_conn.read(1)
                if idx_byte:
                    button_index = idx_byte[0]
                    self.handle_button_press(button_index)

    def write_pid_file(self):
        try:
            with open(self.pid_file, "w") as f:
                f.write(str(os.getpid()))
            self.log(f"PID {os.getpid()} written to {self.pid_file}")
        except IOError as e:
            self.log(f"Error writing PID file: {e}")

    def remove_pid_file(self):
        try:
            if os.path.exists(self.pid_file):
                os.remove(self.pid_file)
        except IOError:
            pass

    def signal_handler(self, signum, frame):
        self.log(f"Received signal {signum}, shutting down...")
        self.running = False

    def daemonize(self):
        if os.fork() > 0:
            sys.exit(0)
        os.setsid()
        if os.fork() > 0:
            sys.exit(0)
        sys.stdout = open(self.log_file, "a")
        sys.stderr = sys.stdout

    def run(self):
        if self.list_ports:
            ports = self.list_available_ports()
            if not ports:
                print("No serial ports found")
            else:
                print("Available Stream Deck serial ports:")
                for p in ports:
                    print(f"  {p.device} - {p.description}")
            return 0
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
        if self.daemon:
            self.daemonize()
            self.write_pid_file()
        self.log("Stream Deck Companion starting...")
        if not self.connect():
            return 1
        self.running = True
        self.log("Listening for button presses...")
        try:
            self.listen_loop()
        except Exception as e:
            self.log(f"Error in listen loop: {e}")
        finally:
            self.disconnect()
            self.remove_pid_file()
            self.log("Stream Deck Companion stopped")
        return 0


def main():
    parser = argparse.ArgumentParser(description="Stream Deck Companion App")
    parser.add_argument("--port", "-p", help="Serial port to connect to (e.g., /dev/cu.usbmodem1102)")
    parser.add_argument("--daemon", "-d", action="store_true", help="Run as background daemon")
    parser.add_argument("--list", "-l", action="store_true", help="List available serial ports")
    args = parser.parse_args()
    companion = StreamDeckCompanion(port=args.port, daemon=args.daemon, list_ports=args.list)
    return companion.run()


if __name__ == "__main__":
    sys.exit(main())
