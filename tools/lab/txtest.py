#!/usr/bin/env python3
"""TX quality test: board A (ESP-SDR) records every CAP16 snapshot while board B
(lab firmware) transmits search-frame copies; then report bit errors per frame,
deviation and in-frame carrier drift (same measurement used for the keyboard)."""
import argparse
import subprocess
import sys
import threading
import time
from pathlib import Path

import numpy as np
from scipy.signal import firwin, lfilter

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/capture"))
sys.path.insert(0, str(ROOT / "tools/analyze_iq"))
from sdr import Sdr  # noqa: E402
import esb_match as M  # noqa: E402

FS = 16e6


def measure(caps, center):
    ref = M.FRAMES["search"]
    tb = np.array([1.0 if b == "1" else -1.0 for b in ref])
    rows = []
    for z in caps:
        for h in M.scan(z, FS, center, 6, 0.25, 0.5):
            if h["frame"] != "search":
                continue
            n = np.arange(len(z))
            bb = lfilter(firwin(63, 1.4e6, fs=FS), 1, z * np.exp(-2j * np.pi * (h["mhz"] - center) * 1e6 / FS * n))
            f = np.angle(bb[1:] * np.conj(bb[:-1])) * FS / 2 / np.pi
            hp = f - np.convolve(f, np.ones(64) / 64, "same")
            k = int(np.argmax(np.correlate(hp, M.TEMPL["search"], "valid")))
            best = None
            for d in range(-8, 9):
                idx = k + d + 4 + 8 * np.arange(len(ref))
                if idx[0] < 0 or idx[-1] >= len(f):
                    continue
                s = f[idx]
                a, b = np.polyfit(tb, s, 1)
                bad = np.flatnonzero(((s - b) > 0) != (tb > 0))
                e = len(bad)
                if best is None or e < best[0]:
                    r = s - a * tb
                    best = (e, a, r[-40:].mean() - r[:40].mean(), h["mhz"] * 1e3 + b / 1e3, list(bad))
            if best:
                rows.append(best)
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--tx-mhz", type=int, default=2443)
    ap.add_argument("--rx-mhz", type=int, default=2440)
    ap.add_argument("--gain", type=int, default=4)
    ap.add_argument("--settle", type=int, default=3000)
    ap.add_argument("--dev", type=int, default=500)
    ap.add_argument("--count", type=int, default=600)
    ap.add_argument("--lead", type=int, default=400)
    ap.add_argument("--chirp", type=int, default=0, help="Hz per us")
    ap.add_argument("--if-khz", type=int, default=0, help="digital IF; LO is tuned to tx-mhz - IF")
    ap.add_argument("--caps", type=int, default=150)
    ap.add_argument("--rx-gain", default="hw", help="board A gain: hw (AGC) or a manual index 0..82")
    a = ap.parse_args()
    lab = [sys.executable, str(ROOT / "tools/lab/lab.py"), "--timeout", "60"]
    lo_khz = a.tx_mhz * 1000 - a.if_khz
    subprocess.run(lab + ["TUNE", str(lo_khz // 1000), str(lo_khz % 1000)], check=True, capture_output=True)
    sdr = Sdr()
    sdr.cmd("GAIN HARDWARE" if a.rx_gain == "hw" else f"GAIN MANUAL {a.rx_gain}")
    sdr.cmd(f"FREQ {a.rx_mhz}")
    caps = []
    t = threading.Thread(target=lambda: subprocess.run(
        lab + ["TXSEARCH", str(a.gain), str(a.count), "1", "40", str(a.settle), str(a.dev), str(a.if_khz), str(a.lead), str(a.chirp)], capture_output=True))
    t.start()
    while t.is_alive() and len(caps) < a.caps:
        iq, _ = sdr.cap16(16380, 6)
        caps.append(iq)
    t.join()
    rows = measure(caps, a.rx_mhz)
    errs = [r[0] for r in rows]
    exact = sum(e == 0 for e in errs)
    print(f"settle={a.settle}us dev={a.dev}kHz if={a.if_khz}kHz lead={a.lead} chirp={a.chirp} gain={a.gain} rxgain={a.rx_gain}: caps={len(caps)} frames={len(rows)} exact={exact} "
          f"errs={sorted(errs)} meas_dev={np.median([r[1] for r in rows])/1e3 if rows else 0:.0f}kHz "
          f"drift={np.median([r[2] for r in rows])/1e3 if rows else 0:+.0f}kHz "
          f"carrier={np.median([r[3] for r in rows]) if rows else 0:.0f}kHz")
    from collections import Counter
    pos = Counter(p for r in rows for p in r[4])
    ref = M.FRAMES["search"]
    print("error positions (bit index: count):", dict(sorted(pos.items())))
    print("context:", {p: ref[max(0, p - 3):p + 4] for p in sorted(pos)})


if __name__ == "__main__":
    main()
