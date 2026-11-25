#!/usr/bin/env python3
"""Send one command line to the D8 lab firmware (board B) and print the reply line(s)."""
import argparse
import sys
import time

import serial

BOARD_B = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_9C:13:9E:AC:5C:F8-if00"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default=BOARD_B)
    ap.add_argument("--timeout", type=float, default=30.0)
    ap.add_argument("cmd", nargs="+")
    a = ap.parse_args()
    s = serial.Serial(a.port, 115200, timeout=0.2)
    time.sleep(0.1)
    s.reset_input_buffer()
    s.write((" ".join(a.cmd) + "\n").encode())
    deadline = time.time() + a.timeout
    while time.time() < deadline:
        line = s.readline().decode(errors="replace").strip()
        if not line:
            continue
        print(line)
        if line.startswith(("OK", "ERR")):
            return 0 if line.startswith("OK") else 1
    print("ERR timeout", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
