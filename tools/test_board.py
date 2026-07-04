#!/usr/bin/env python3
"""Basic board test for saramOS serial console."""

import sys
import time
import serial

PORT = "/dev/ttyACM0"
BAUD = 115200
TIMEOUT = 0.5


def read_all(ser, duration=1.0):
    """Read whatever is available for a short time."""
    end = time.time() + duration
    data = b""
    while time.time() < end:
        avail = ser.in_waiting
        if avail:
            data += ser.read(avail)
        else:
            time.sleep(0.05)
    return data.decode("utf-8", errors="replace")


def send_cmd(ser, cmd, wait=1.5):
    ser.write((cmd + "\r\n").encode("utf-8"))
    time.sleep(0.1)
    return read_all(ser, wait)


def main():
    print(f"Opening {PORT} @ {BAUD}...")
    with serial.Serial(PORT, BAUD, timeout=TIMEOUT) as ser:
        time.sleep(0.2)
        ser.reset_input_buffer()

        print("\n--- boot output ---")
        print(read_all(ser, 1.5))

        print("\n--- help ---")
        print(send_cmd(ser, "help", 1.0))

        print("\n--- sd init ---")
        print(send_cmd(ser, "sd init", 3.0))

        print("\n--- sd ls / ---")
        print(send_cmd(ser, "sd ls /", 2.0))

        print("\n--- net init ---")
        print(send_cmd(ser, "net init", 2.0))

        print("\n--- net status (wait for DHCP) ---")
        for _ in range(6):
            out = send_cmd(ser, "net status", 2.0)
            print(out)
            if "IP" in out:
                break
            time.sleep(1.0)

        print("\n--- http start ---")
        print(send_cmd(ser, "http start", 1.0))

    print("\nDone.")


if __name__ == "__main__":
    main()
