#!/usr/bin/env python3
"""Capture serial output from saramOS during SD debug."""
import sys
import time
import serial

PORT = "/dev/ttyACM0"
BAUD = 115200
TIMEOUT = 0.1


def wait_for_prompt(ser, timeout=10.0):
    """Read until we see the saramOS prompt."""
    start = time.time()
    buf = b""
    while time.time() - start < timeout:
        data = ser.read(ser.in_waiting or 1)
        if data:
            buf += data
            if b"saramOS> " in buf:
                return buf
    return buf


def send_cmd(ser, cmd, wait_after=0.5):
    ser.write((cmd + "\r").encode())
    time.sleep(wait_after)
    out = b""
    deadline = time.time() + 5.0
    while time.time() < deadline:
        data = ser.read(ser.in_waiting or 1)
        if data:
            out += data
            if b"saramOS> " in out:
                break
        else:
            time.sleep(0.05)
    return out


def main():
    with serial.Serial(PORT, BAUD, timeout=TIMEOUT) as ser:
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        print("Waiting for board boot...")
        boot = wait_for_prompt(ser, timeout=15.0)
        print(boot.decode("utf-8", errors="replace"))

        print("\n--- sending: sd init ---")
        out = send_cmd(ser, "sd init", wait_after=1.0)
        print(out.decode("utf-8", errors="replace"))

        print("\n--- sending: sd inspect ---")
        out = send_cmd(ser, "sd inspect", wait_after=0.5)
        print(out.decode("utf-8", errors="replace"))

        print("\n--- sending: sd info ---")
        out = send_cmd(ser, "sd info", wait_after=0.5)
        print(out.decode("utf-8", errors="replace"))

        print("\n--- sending: sd ls / ---")
        out = send_cmd(ser, "sd ls /", wait_after=1.0)
        print(out.decode("utf-8", errors="replace"))

        print("\n--- sending: sd cat /README.md ---")
        out = send_cmd(ser, "sd cat /README.md", wait_after=1.0)
        print(out.decode("utf-8", errors="replace"))


if __name__ == "__main__":
    main()
