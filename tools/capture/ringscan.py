#!/usr/bin/env python3
"""Wideband gap-free bursts (RINGCAP) to measure keyboard attempt timing and hop order.

Each RINGCAP at 40 MS/s gives ~1.2 ms of gap-free I/Q covering ~±20 MHz. For each
capture we detect bursts, estimate their carrier (spectral centroid relative to
centre) and start time, and save everything for offline analysis. RX only.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from sdr import Sdr  # noqa: E402

RATE_CODE = {16: 6, 40: 1, 80: 0}


def bursts(iq: np.ndarray, fs: float, thresh_db: float, min_us: float, win: int):
    p = np.convolve(np.abs(iq) ** 2, np.ones(win) / win, mode="same")
    floor = np.median(p) + 1e-9
    on = p > floor * 10 ** (thresh_db / 10)
    d = np.diff(np.concatenate(([0], on.astype(np.int8), [0])))
    starts, ends = np.flatnonzero(d == 1), np.flatnonzero(d == -1)
    out = []
    for s, e in zip(starts, ends):
        if (e - s) / fs * 1e6 < min_us:
            continue
        seg = iq[s:e] - iq[s:e].mean()
        n = 1 << int(np.ceil(np.log2(max(len(seg), 64))))
        spec = np.abs(np.fft.fftshift(np.fft.fft(seg, n))) ** 2
        f = np.fft.fftshift(np.fft.fftfreq(n, 1 / fs))
        fc = float((spec * f).sum() / spec.sum())
        out.append(dict(t_us=s / fs * 1e6, dur_us=(e - s) / fs * 1e6, f_off_mhz=fc / 1e6,
                        snr_db=float(10 * np.log10(p[s:e].mean() / floor))))
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default=None)
    ap.add_argument("--center", type=int, default=2462)
    ap.add_argument("--msps", type=int, default=40, choices=sorted(RATE_CODE))
    ap.add_argument("--bandwidth", type=int, default=40)
    ap.add_argument("--caps", type=int, default=300)
    ap.add_argument("--thresh-db", type=float, default=10)
    ap.add_argument("--min-us", type=float, default=40)
    ap.add_argument("--label", default="ring")
    ap.add_argument("--save-dir", type=Path)
    ap.add_argument("--conj", type=int, default=1)
    a = ap.parse_args()
    fs = a.msps * 1e6
    sdr = Sdr(a.port) if a.port else Sdr()
    sdr.cmd("GAIN HARDWARE")
    sdr.cmd(f"BANDWIDTH {a.bandwidth}")
    sdr.cmd(f"FREQ {a.center}")
    rows = []
    t0 = time.time()
    for i in range(a.caps):
        iq, counts, _rep = sdr.ringcap(3, RATE_CODE[a.msps], conj=bool(a.conj))
        bs = bursts(iq, fs, a.thresh_db, a.min_us, win=max(8, int(fs / 2e6)))
        rows.append(dict(i=i, host_t=time.time(), n=len(iq), bursts=bs))
        if a.save_dir and len(bs) >= 2:
            a.save_dir.mkdir(parents=True, exist_ok=True)
            np.save(a.save_dir / f"{a.label}_{i:04d}.npy", iq.astype(np.complex64))
    meta = dict(label=a.label, center_mhz=a.center, fs=fs, bandwidth=a.bandwidth,
                elapsed_s=time.time() - t0, caps=rows)
    if a.save_dir:
        a.save_dir.mkdir(parents=True, exist_ok=True)
        (a.save_dir / f"{a.label}_meta.json").write_text(json.dumps(meta))
    multi = [r for r in rows if len(r["bursts"]) >= 2]
    print(f"# {a.label}: {a.caps} caps in {meta['elapsed_s']:.1f}s; caps with >=2 bursts: {len(multi)}")
    for r in multi[:40]:
        print(r["i"], " | ".join(f"t={b['t_us']:7.1f} d={b['dur_us']:5.1f} f={a.center + b['f_off_mhz']:7.2f}"
                                 for b in r["bursts"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
