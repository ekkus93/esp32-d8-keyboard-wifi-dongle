#!/usr/bin/env python3
"""Demodulate 2 Mbps GFSK-like bursts to bits (8 samples/bit @16 MS/s).

For each qualifying burst: mix to baseband, FM-discriminate, pick the sampling
phase with the widest eye, slice, and print bits. Optionally search for the
longest common substring across bursts (sync-word candidate).
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from inspect_bursts import FS, analyze, fm, lowpass  # noqa: E402

SPS = 8


def demod_bits(iq, s, e, fc, pad=64):
    s0, e0 = max(0, s - pad), min(len(iq), e + pad)
    n = np.arange(s0, e0)
    seg = lowpass(iq[s0:e0] * np.exp(-2j * np.pi * fc / FS * n), 5)
    f = lowpass(fm(seg), 3)
    core = f[pad: pad + (e - s)]
    f0 = np.median(core)
    d = f - f0
    best = None
    for ph in range(SPS):
        smp = d[pad + ph: pad + (e - s): SPS]
        eye = np.percentile(np.abs(smp), 20)
        if best is None or eye > best[0]:
            best = (eye, ph, smp)
    _, ph, smp = best
    return (smp > 0).astype(np.uint8), f0


def bits2hex(b):
    b = b[: len(b) // 8 * 8]
    return bytes(np.packbits(b)).hex()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+", type=Path)
    ap.add_argument("--min-us", type=float, default=15)
    ap.add_argument("--mhz", type=float, default=2476.0)
    ap.add_argument("--tol", type=float, default=0.5)
    ap.add_argument("--out", type=Path)
    a = ap.parse_args()
    rows = []
    for d in a.dirs:
        for js in sorted(d.glob("*.json")):
            m = json.loads(js.read_text())
            iq = np.load(js.with_suffix(".npy"))
            for s, e, fc, snr in m["bursts"]:
                if (e - s) / FS * 1e6 < a.min_us:
                    continue
                r = analyze(iq, s, e, fc)
                if r is None or not len(r["runs"]):
                    continue
                mr = np.median(r["runs"])
                car = m["center_mhz"] + (fc + r["f0_khz"] * 1e3) / 1e6
                if not (6 <= mr <= 12 and 350 <= r["dev_khz"] <= 800 and abs(car - a.mhz) <= a.tol):
                    continue
                bits, _ = demod_bits(iq, s, e, fc)
                rows.append(dict(cap=js.stem, t=m["time"], s=int(s), snr=float(snr),
                                 car=float(car), us=(e - s) / FS * 1e6,
                                 bits="".join(map(str, bits))))
    rows.sort(key=lambda r: (r["t"], r["s"]))
    for r in rows:
        print(f"{r['cap']:32s} {r['car']:.3f} {r['us']:5.1f}us snr={r['snr']:4.1f} {r['bits']}")
    if a.out:
        a.out.write_text(json.dumps(rows, indent=0))
    return 0


if __name__ == "__main__":
    sys.exit(main())
