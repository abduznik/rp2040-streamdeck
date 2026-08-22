#!/usr/bin/env python3
"""Quick CLI test for StreamDeck serial communication"""
import serial, sys, time

SOF = 0xAA
CMD_PING = 0x04
CMD_GET_MAPPING = 0x01
REPLY_PONG = 0x84
REPLY_MAPPING = 0x81
ACTION_SIZE = 283
ACTION_NAMES = {0:'None',1:'Key',2:'Consumer',3:'Macro',4:'Text',5:'Paste',6:'Launcher'}

port_name = sys.argv[1] if len(sys.argv) > 1 else '/dev/cu.usbmodem1102'
print(f"Opening {port_name}...")
s = serial.Serial(port_name, 115200, timeout=3)
time.sleep(0.5)

# Drain stale data
s.reset_input_buffer()

def send_and_read(cmd, payload=b'', expected_reply=None):
    cs = cmd
    for b in payload: cs = (cs + b) & 0xFF
    pkt = bytes([SOF, cmd, len(payload) & 0xFF, (len(payload) >> 8) & 0xFF]) + payload + bytes([cs])
    s.write(pkt)
    s.flush()
    if expected_reply is None: return None
    # Read reply looking for SOF
    buf = b''
    deadline = time.time() + 3
    while time.time() < deadline:
        chunk = s.read(4096)
        if not chunk: continue
        buf += chunk
        while True:
            idx = buf.find(SOF)
            if idx < 0: buf = b''; break
            if idx > 0: buf = buf[idx:]
            if len(buf) < 4: break
            cmd_r = buf[1]
            length = buf[2] | (buf[3] << 8)
            if len(buf) < 4 + length + 1: break
            payload_r = buf[4:4+length]
            buf = buf[4+length+1:]
            if cmd_r == expected_reply:
                return payload_r
    return None

# Test PING
print("\n--- PING ---")
pong = send_and_read(CMD_PING, expected_reply=REPLY_PONG)
if pong:
    print(f"PONG: fw_version={pong[0] if pong else '?'}")
else:
    print("NO PONG REPLY")

# Test GET_MAPPING
print("\n--- GET_MAPPING ---")
mapping = send_and_read(CMD_GET_MAPPING, expected_reply=REPLY_MAPPING)
if mapping:
    print(f"Got {len(mapping)} bytes")
    for i in range(6):
        off = i * ACTION_SIZE
        if off + ACTION_SIZE > len(mapping): break
        t = mapping[off]
        lb = mapping[off+1+258:off+1+258+24]
        label = lb.split(b'\x00')[0].decode('utf-8', errors='replace')
        type_name = ACTION_NAMES.get(t, f'???({t})')
        print(f"  Button {i}: type={type_name} label='{label}'")
else:
    print("NO MAPPING REPLY")

s.close()
print("\nDone.")
