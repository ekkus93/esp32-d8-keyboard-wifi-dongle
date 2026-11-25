#!/usr/bin/env python3
"""Inspect saved burst captures: FM-demodulate bursts near target MHz, estimate
frequency deviation and symbol rate (from zero-crossing interval histogram)."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

import numpy as np

FS = 16e6


def fm(iq: np.ndarray) -> np.ndarray:
    return np.angle(iq[1:] * np.conj(iq[:-1])) * FS / (2 * np.pi)


def lowpass(x: np.ndarray, n: int) -> np.ndarray:
    return np.convolve(x, np.ones(n) / n, mode="same")


def analyze(iq, s, e, fc):
    seg = iq[s:e] * np.exp(-2j * np.pi * fc / FS * np.arange(s, e))
    # channel filter ~ +-1.5 MHz (boxcar 5 taps @16 MS/s ~ 3.2 MHz null)
    seg = lowpass(seg, 5)
    f = fm(seg)
    f = lowpass(f, 2)
    f = f[8:-8]
    if len(f) < 64:
        return None
    f0 = np.median(f)
    d = f - f0
    dev = np.percentile(np.abs(d), 80)
    sgn = np.sign(d)
    zc = np.flatnonzero(np.diff(sgn) != 0)
    runs = np.diff(zc)
    return dict(len_us=(e - s) / FS * 1e6, f0_khz=f0 / 1e3, dev_khz=dev / 1e3,
                runs=runs, bits=(sgn > 0).astype(int))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("dir", type=Path)
    ap.add_argument("--mhz", default="2444,2454,2465,2466,2476")
    ap.add_argument("--min-us", type=float, default=10)
    ap.add_argument("--show-bits", type=int, default=0)
    a = ap.parse_args()
    targets = {int(x) for x in a.mhz.split(",")}
    allruns = []
    rows = []
    for js in sorted(a.dir.glob("*.json")):
        meta = json.loads(js.read_text())
        iq = np.load(js.with_suffix(".npy"))
        c = meta["center_mhz"]
        for s, e, fc, snr in meta["bursts"]:
            mhz = round(c + fc / 1e6)
            if mhz not in targets or (e - s) / FS * 1e6 < a.min_us:
                continue
            r = analyze(iq, s, e, fc)
            if r is None:
                continue
            rows.append((js.stem, mhz, snr, r))
            allruns.extend(r["runs"].tolist())
    print(f"{len(rows)} bursts")
    for name, mhz, snr, r in rows:
        rl = r["runs"]
        print(f"{name:28s} {mhz} snr={snr:5.1f} len={r['len_us']:6.1f}us f0={r['f0_khz']:7.0f}kHz "
              f"dev={r['dev_khz']:5.0f}kHz minrun={rl.min() if len(rl) else 0} medrun={np.median(rl) if len(rl) else 0:.0f}")
        if a.show_bits:
            b = r["bits"]
            print("   ", "".join(map(str, b[: a.show_bits])))
    if allruns:
        h = np.bincount(np.clip(allruns, 0, 80))
        print("run-length histogram (samples @16MS/s):", {i: int(v) for i, v in enumerate(h) if v})
    return 0


if __name__ == "__main__":
    sys.exit(main())
