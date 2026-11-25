#!/usr/bin/env python3
"""Find the D8 search/power-on frames in wideband captures by matched filtering.

The keyboard's frames are fully known (docs/RF_PROTOCOL.md D1-D3, D8), so we
correlate the FM-discriminator output against the known NRZ bit pattern of
access code + header + payload + CRC at every candidate channel. Output: exact
start time (of the access code) and carrier for each detected frame.
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np
from scipy.signal import firwin, lfilter, resample_poly

ADDR = "0101010101011001010101010101010110001110"
HDR = "001011001"


def crc16(bits: str, init: int = 0xFFFF) -> int:
    c = init
    for b in bits:
        x = ((c >> 15) & 1) ^ int(b)
        c = (c << 1) & 0xFFFF
        if x:
            c ^= 0x1021
    return c


def frame_bits(flow: int) -> str:
    pay = [0x51, flow, 0x01, 0, 0, 0, 0, 0x0C, 0, 0, 0x10]
    body = ADDR + HDR + "".join(format(b, "08b") for b in pay)
    return body + format(crc16(body), "016b")


FRAMES = {"search": frame_bits(0x40), "poweron": frame_bits(0x50)}
SPS = 8           # samples per bit at 16 MS/s
FS_OUT = 16e6


def template(bits: str) -> np.ndarray:
    t = np.repeat(np.array([1.0 if b == "1" else -1.0 for b in bits]), SPS)
    return (t - t.mean()) / np.linalg.norm(t)


TEMPL = {k: template(v) for k, v in FRAMES.items()}


def to_16msps(iq: np.ndarray, fs: float) -> np.ndarray:
    if fs == 16e6:
        return iq
    if fs == 40e6:
        return resample_poly(iq, 2, 5)
    if fs == 80e6:
        return resample_poly(iq, 1, 5)
    raise ValueError(fs)


def scan(iq: np.ndarray, fs: float, center_mhz: float, span_mhz: float, step_mhz: float, thresh: float):
    n = np.arange(len(iq))
    lp = firwin(63, 1.4e6, fs=fs)
    hits = []
    for off in np.arange(-span_mhz, span_mhz + 1e-9, step_mhz):
        bb = iq * np.exp(-2j * np.pi * off * 1e6 / fs * n)
        bb = lfilter(lp, 1, bb)
        z = to_16msps(bb, fs)
        f = np.angle(z[1:] * np.conj(z[:-1]))
        f = f - np.convolve(f, np.ones(64) / 64, mode="same")   # remove carrier offset
        for name, t in TEMPL.items():
            if len(f) < len(t):
                continue
            c = np.correlate(f, t, mode="valid")
            norm = np.sqrt(np.convolve(f * f, np.ones(len(t)), mode="valid")) + 1e-9
            r = c / norm
            k = int(np.argmax(r))
            if r[k] > thresh:
                hits.append(dict(frame=name, t_us=(k * (1 if fs == FS_OUT else 1)) / FS_OUT * 1e6,
                                 mhz=center_mhz + off, score=float(r[k])))
    # keep best per (frame, time cluster)
    hits.sort(key=lambda h: -h["score"])
    kept = []
    for h in hits:
        if all(abs(h["t_us"] - k["t_us"]) > 20 for k in kept):
            kept.append(h)
    return sorted(kept, key=lambda h: h["t_us"])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("dir", type=Path)
    ap.add_argument("--meta", type=Path, help="ringscan *_meta.json (centre, fs)")
    ap.add_argument("--span", type=float, default=19.0)
    ap.add_argument("--step", type=float, default=0.25)
    ap.add_argument("--thresh", type=float, default=0.6)
    ap.add_argument("--out", type=Path)
    a = ap.parse_args()
    meta = json.loads((a.meta or next(a.dir.glob("*_meta.json"))).read_text())
    fs, c = meta["fs"], meta["center_mhz"]
    host_t = {r["i"]: r["host_t"] for r in meta["caps"]}
    res = []
    for f in sorted(a.dir.glob("*.npy")):
        i = int(f.stem.split("_")[-1])
        iq = np.load(f)
        hits = scan(iq, fs, c, a.span, a.step, a.thresh)
        res.append(dict(cap=i, host_t=host_t.get(i), hits=hits))
        if hits:
            print(i, " | ".join(f"{h['frame'][0]} t={h['t_us']:7.1f} f={h['mhz']:7.2f} r={h['score']:.2f}" for h in hits))
    if a.out:
        a.out.write_text(json.dumps(res))
    return 0


if __name__ == "__main__":
    sys.exit(main())
