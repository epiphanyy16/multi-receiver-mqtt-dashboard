#!/usr/bin/env python3
"""
Serial Monitor for BLE Emergency Switch Demo (Laptop A)

Reads Serial output from Arduino Nano 33 IoT and displays it.
The Arduino handles MQTT publishing directly — this script is
a serial monitor only (no duplicate publishing).

Features:
  - Auto-detect serial port if --port is not specified
  - Reconnect automatically if the Arduino is unplugged/re-plugged

Usage:
    python serial_to_mqtt.py                          # auto-detect port
    python serial_to_mqtt.py --port /dev/cu.usbmodem14101
"""

import argparse
import glob
import sys
import time
import serial


# --- Configuration Defaults ---
DEFAULT_BAUD = 115200


def find_serial_port():
    """Auto-detect the Arduino serial port."""
    patterns = [
        "/dev/cu.usbmodem*",   # macOS
        "/dev/ttyACM*",        # Linux
        "/dev/ttyUSB*",        # Linux (some boards)
    ]

    candidates = []
    for pattern in patterns:
        candidates.extend(glob.glob(pattern))

    if not candidates:
        return None

    if len(candidates) == 1:
        return candidates[0]

    # Multiple ports found — pick the first, but warn
    print(f"[SERIAL] Multiple ports found: {candidates}")
    print(f"[SERIAL] Using: {candidates[0]}  (use --port to override)")
    return candidates[0]


def parse_args():
    parser = argparse.ArgumentParser(
        description="Serial monitor for Arduino Nano 33 IoT"
    )
    parser.add_argument(
        "--port", default=None,
        help="Serial port (e.g. /dev/cu.usbmodem14101). Auto-detected if not specified."
    )
    parser.add_argument(
        "--baud", type=int, default=DEFAULT_BAUD,
        help=f"Serial baud rate (default: {DEFAULT_BAUD})"
    )
    return parser.parse_args()


def open_serial(port, baud):
    """Open serial port with retries."""
    while True:
        try:
            ser = serial.Serial(port, baud, timeout=1)
            return ser
        except serial.SerialException as e:
            print(f"[SERIAL] Cannot open {port}: {e}")
            print(f"[SERIAL] Retrying in 3 seconds... (is the Arduino plugged in?)")
            time.sleep(3)


def main():
    args = parse_args()

    # --- Find serial port ---
    port = args.port
    if port is None:
        print("[SERIAL] Auto-detecting serial port...")
        port = find_serial_port()
        if port is None:
            print("[SERIAL] ERROR: No serial port found.")
            print("[SERIAL] Make sure the Arduino is plugged in via USB.")
            print("[SERIAL] On macOS: look for /dev/cu.usbmodem*")
            print("[SERIAL] On Linux:  look for /dev/ttyACM*")
            print("[SERIAL] Or specify manually: --port /dev/cu.usbmodem14101")
            sys.exit(1)
        print(f"[SERIAL] Found: {port}")

    # --- Open Serial port ---
    print(f"[SERIAL] Opening {port} at {args.baud} baud...")
    ser = open_serial(port, args.baud)

    print(f"[SERIAL] Port opened. Waiting for data...\n")
    print("=" * 60)
    print("  Serial Monitor Active")
    print(f"  Port: {port}  |  Baud: {args.baud}")
    print("  (Arduino publishes to MQTT directly)")
    print("=" * 60)
    print()

    try:
        while True:
            try:
                raw = ser.readline()
            except serial.SerialException:
                print(f"\n[SERIAL] Connection lost. Reconnecting...")
                ser.close()
                time.sleep(2)
                ser = open_serial(port, args.baud)
                print(f"[SERIAL] Reconnected to {port}\n")
                continue

            if not raw:
                continue

            # Decode and strip whitespace
            try:
                line = raw.decode("utf-8", errors="replace").strip()
            except Exception:
                continue

            if not line:
                continue

            # Print with timestamp (serial monitor behavior)
            timestamp = time.strftime("%H:%M:%S")
            print(f"[{timestamp}] {line}")

    except KeyboardInterrupt:
        print("\n[INFO] Shutting down...")
    finally:
        ser.close()
        print("[INFO] Serial port closed.")


if __name__ == "__main__":
    main()
