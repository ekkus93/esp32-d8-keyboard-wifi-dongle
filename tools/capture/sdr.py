#!/usr/bin/env python3
"""Minimal host client for ESP-SDR burst firmware (external, GPL; protocol only).

RX-only. Captures are (I, -Q) on the wire; this client returns standard complex I+jQ.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
import zlib
from pathlib import Path

import numpy as np
import serial

RATES_MSPS = {0: 80, 1: 40, 2: 20, 3: 10, 4: 8, 5: 4, 6: 16, 7: 32, 8: 10.667, 9: 6.4}


class Sdr:
    def __init__(self, port: str = "/dev/ttyACM0", timeout: float = 3.0):
        self.s = serial.Serial(port, 2_000_000, timeout=timeout)
        self.sync()

    def sync(self) -> None:
        nonce = str(int(time.time() * 1000) % 1_000_000)
        self.s.reset_input_buffer()
        self.s.write(f"SYNC {nonce}\n".encode())
        deadline = time.time() + 5
        while time.time() < deadline:
            line = self.s.readline().decode(errors="replace").strip()
            if line == f"SYNC {nonce}":
                return
        raise RuntimeError("no SYNC echo")

    def cmd(self, c: str) -> str:
        self.s.write((c + "\n").encode())
        line = self.s.readline().decode(errors="replace").strip()
        if not line:
            raise RuntimeError(f"timeout on {c!r}")
        if line.startswith("ERR"):
            raise RuntimeError(f"{c!r}: {line}")
        return line

    def _read_exact(self, n: int) -> bytes:
        buf = bytearray()
        while len(buf) < n:
            chunk = self.s.read(n - len(buf))
            if not chunk:
                raise RuntimeError(f"short read {len(buf)}/{n}")
            buf += chunk
        return bytes(buf)

    def cap16(self, n: int, rate: int = 6) -> tuple[np.ndarray, int]:
        hdr = self.cmd(f"CAP16 {n} {rate}")
        parts = hdr.split()
        if parts[0] != "DATA":
            raise RuntimeError(hdr)
        count, crc, us = int(parts[1]), int(parts[2], 16), int(parts[3])
        raw = self._read_exact(count * 2)
        if zlib.crc32(raw) & 0xFFFFFFFF != crc:
            raise RuntimeError("CRC mismatch")
        a = np.frombuffer(raw, dtype=np.int8).astype(np.float32).reshape(-1, 2)
        return a[:, 0] - 1j * a[:, 1], us  # negate wire Q


def save(path: Path, iq: np.ndarray, meta: dict) -> None:
    np.save(path.with_suffix(".npy"), iq.astype(np.complex64))
    path.with_suffix(".json").write_text(json.dumps(meta, indent=1))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default="/dev/ttyACM0")
    ap.add_argument("cmd", nargs="+", help="raw protocol command, e.g. INFO or 'LIMITS?'")
    a = ap.parse_args()
    sdr = Sdr(a.port)
    print(sdr.cmd(" ".join(a.cmd)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
