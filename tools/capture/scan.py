#!/usr/bin/env python3
"""Band survey: repeated 16 MS/s snapshots at several centres; report bursts per 1 MHz bin.

A 'burst' is a run of samples whose short-window power exceeds the capture's
noise floor by --thresh-db. For each burst we estimate its centre frequency
(spectral centroid) and duration, and bin it to the nearest MHz.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from collections import defaultdict
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from sdr import Sdr, save  # noqa: E402

FS = 16e6


def bursts(iq: np.ndarray, thresh_db: float, win: int = 32):
    p = np.convolve(np.abs(iq) ** 2, np.ones(win) / win, mode="same")
    floor = np.median(p) + 1e-9
    on = p > floor * 10 ** (thresh_db / 10)
    edges = np.flatnonzero(np.diff(on.astype(np.int8)))
    starts = list(edges[~on[edges]] + 1) if len(edges) else []
    if on[0]:
        starts = [0] + starts
    out = []
    for s in starts:
        e = s + int(np.argmax(~on[s:])) if not on[s:].all() else len(on)
        if e - s < 16:  # < 1 us: ignore
            continue
        seg = iq[s:e] - iq[s:e].mean()
        n = 1 << int(np.ceil(np.log2(max(len(seg), 64))))
        spec = np.abs(np.fft.fftshift(np.fft.fft(seg, n))) ** 2
        f = np.fft.fftshift(np.fft.fftfreq(n, 1 / FS))
        fc = float((spec * f).sum() / spec.sum())
        snr = 10 * np.log10(p[s:e].mean() / floor)
        out.append((s, e, fc, snr))
    return out, 10 * np.log10(floor)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--centers", default="2412,2424,2436,2448,2460,2472")
    ap.add_argument("--caps", type=int, default=40, help="snapshots per centre")
    ap.add_argument("--gain", default="HARDWARE")
    ap.add_argument("--thresh-db", type=float, default=10)
    ap.add_argument("--min-us", type=float, default=10)
    ap.add_argument("--label", default="ambient")
    ap.add_argument("--save-dir", type=Path)
    ap.add_argument("--save-max", type=int, default=20)
    a = ap.parse_args()

    sdr = Sdr()
    sdr.cmd("GAIN HARDWARE" if a.gain == "HARDWARE" else f"GAIN MANUAL {a.gain}")
    hist: dict[int, list] = defaultdict(list)
    saved = 0
    t0 = time.time()
    floors = {}
    for c in [int(x) for x in a.centers.split(",")]:
        sdr.cmd(f"FREQ {c}")
        fl = []
        for i in range(a.caps):
            iq, _us = sdr.cap16(16380, 6)
            bs, floor = bursts(iq, a.thresh_db)
            fl.append(floor)
            kept = []
            for s, e, fc, snr in bs:
                mhz = round(c + fc / 1e6)
                if abs(fc) > 6.5e6 or (e - s) / FS * 1e6 < a.min_us:
                    continue
                hist[mhz].append(((e - s) / FS * 1e6, snr))
                kept.append((s, e, fc, snr))
            bs = kept
            if bs and a.save_dir and saved < a.save_max:
                a.save_dir.mkdir(parents=True, exist_ok=True)
                save(a.save_dir / f"{a.label}_{c}_{i:03d}", iq,
                     {"center_mhz": c, "fs": FS, "label": a.label, "gain": a.gain,
                      "time": time.time(), "bursts": [(int(s), int(e), fc, snr) for s, e, fc, snr in bs]})
                saved += 1
        floors[c] = round(float(np.median(fl)), 1)
    dt = time.time() - t0
    print(f"# label={a.label} caps/centre={a.caps} elapsed={dt:.1f}s floors_dB={floors}")
    print("MHz  bursts  med_dur_us  max_dur_us  med_snr_dB")
    for mhz in sorted(hist):
        d = np.array(hist[mhz])
        print(f"{mhz}  {len(d):5d}  {np.median(d[:,0]):9.1f}  {d[:,0].max():9.1f}  {np.median(d[:,1]):8.1f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
