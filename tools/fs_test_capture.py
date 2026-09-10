#!/usr/bin/env python3
"""Capture serial output from saramOS during fsutils/shell/vi debug."""
import sys
import time
import serial

PORT = "/dev/ttyACM0"
BAUD = 115200
TIMEOUT = 0.1


def wait_for_prompt(ser, timeout=10.0):
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


def send_shell_cmd(ser, cmd, wait_after=0.5):
    ser.write((cmd + "\r").encode())
    time.sleep(wait_after)
    out = b""
    deadline = time.time() + 5.0
    while time.time() < deadline:
        data = ser.read(ser.in_waiting or 1)
        if data:
            out += data
            if b"$ " in out:
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

        for cmd, wa in [
            ("sd init", 1.5),
            ("sd mkdir /testdir", 0.5),
            ("sd tee /testdir/a.txt hello world", 0.5),
            ("sd cat /testdir/a.txt", 0.5),
            ("sd echo this is a test", 0.5),
            ("sd rm /testdir/a.txt", 0.5),
        ]:
            print(f"\n--- sending: {cmd} ---")
            out = send_cmd(ser, cmd, wait_after=wa)
            print(out.decode("utf-8", errors="replace"))

        print("\n--- sending: sd mountfs ---")
        ser.write(b"sd mountfs\r")
        time.sleep(1.0)
        print(ser.read(ser.in_waiting).decode("utf-8", errors="replace"))

        for shcmd, wa in [
            ("ls /", 0.5),
            ("mkdir shelltest", 0.5),
            ("echo hi from shell", 0.5),
            ("tee /shelltest/x.txt line one", 0.5),
            ("cat /shelltest/x.txt", 0.5),
            ("rm /shelltest/x.txt", 0.5),
            ("pwd", 0.5),
            ("exit", 0.5),
        ]:
            print(f"\n--- shell cmd: {shcmd} ---")
            out = send_shell_cmd(ser, shcmd, wait_after=wa)
            print(out.decode("utf-8", errors="replace"))

        print("\n--- sending: sd mountfs (coreutils + enhanced shell) ---")
        ser.write(b"sd mountfs\r")
        time.sleep(1.0)
        print(ser.read(ser.in_waiting).decode("utf-8", errors="replace"))

        for shcmd, wa in [
            ("touch /testdir/c.txt", 0.5),
            ("ls -l /testdir", 0.5),
            ("cp /testdir/c.txt /testdir/d.txt", 0.5),
            ("mv /testdir/d.txt /testdir/e.txt", 0.5),
            ("tee /testdir/f.txt one two three", 0.5),
            ("wc -l /testdir/f.txt", 0.5),
            ("grep two /testdir/f.txt", 0.5),
            ("sort /testdir/f.txt", 0.5),
            ("seq 1 5", 0.5),
            ("export GREET=hello", 0.5),
            ("env", 0.5),
            ("echo $GREET world", 0.5),
            ("for f in /testdir/*.txt; do echo file: $f; done", 0.8),
            ("if test -f /testdir/c.txt; then echo exists; fi", 0.8),
            ("rm /testdir/e.txt /testdir/f.txt", 0.5),
            ("exit", 0.5),
        ]:
            print(f"\n--- shell cmd: {shcmd} ---")
            out = send_shell_cmd(ser, shcmd, wait_after=wa)
            print(out.decode("utf-8", errors="replace"))

        print("\n--- sending: sd vi /testdir/b.txt ---")
        ser.write(b"sd vi /testdir/b.txt\r")
        time.sleep(0.5)
        print(ser.read(ser.in_waiting).decode("utf-8", errors="replace"))

        for vicmd, wa in [
            ("i", 0.3),
            ("first line", 0.3),
            ("a", 0.3),
            ("second line", 0.3),
            (":wq", 0.5),
        ]:
            print(f"\n--- vi cmd: {vicmd} ---")
            ser.write((vicmd + "\r").encode())
            time.sleep(wa)
            print(ser.read(ser.in_waiting).decode("utf-8", errors="replace"))

        print("\n--- sending: sd cat /testdir/b.txt ---")
        out = send_cmd(ser, "sd cat /testdir/b.txt", wait_after=0.5)
        print(out.decode("utf-8", errors="replace"))


if __name__ == "__main__":
    main()
