#!/usr/bin/env python3
"""Slice 2 Mbps GFSK bursts using a preamble-derived carrier, and align frames at
the end of the alternating preamble.

Steps per burst: coarse mix to --mhz, FM discriminate, find the longest run of
alternating sign at 8 samples/bit (preamble), take its mean as carrier offset
and its best phase as bit timing, slice, then output the bits that follow the
preamble (starting with the last two preamble bits for context).
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import Counter
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from inspect_bursts import FS, fm, lowpass  # noqa: E402

SPS = 8


def frame_bits(iq, s, e, center_mhz, mhz, pad=48):
    s0, e0 = max(0, s - pad), min(len(iq), e + pad)
    n = np.arange(s0, e0)
    off = (mhz - center_mhz) * 1e6
    seg = lowpass(iq[s0:e0] * np.exp(-2j * np.pi * off / FS * n), 5)
    f = lowpass(fm(seg), 3)
    best = None
    for ph in range(SPS):
        smp = f[ph::SPS]
        # preamble: longest run where successive samples alternate around local mean
        d = np.diff(smp)
        alt = (d[1:] * d[:-1]) < 0
        big = np.abs(d[1:]) > 300e3
        ok = alt & big
        run, bestrun, end = 0, 0, 0
        for i, v in enumerate(ok):
            run = run + 1 if v else 0
            if run > bestrun:
                bestrun, end = run, i
        if best is None or bestrun > best[0]:
            best = (bestrun, ph, end, smp)
    bestrun, ph, end, smp = best
    if bestrun < 16:
        return None
    pre = smp[end - bestrun + 2: end + 2]
    f0 = pre.mean()
    bits = (smp - f0 > 0).astype(np.uint8)
    # end of preamble: first index after the run where alternation breaks
    k = end + 2
    return dict(pre_len=bestrun + 2, f0_khz=float(f0 / 1e3), phase=ph,
                bits=bits, post=bits[k - 2:], dev_khz=float(np.abs(pre - f0).mean() / 1e3))


def b2h(b):
    b = np.asarray(b[: len(b) // 8 * 8])
    return bytes(np.packbits(b)).hex() if len(b) else ""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("dirs", nargs="+", type=Path)
    ap.add_argument("--mhz", type=float, default=2475.85)
    ap.add_argument("--min-us", type=float, default=15)
    ap.add_argument("--nbits", type=int, default=160)
    ap.add_argument("--out", type=Path)
    a = ap.parse_args()
    rows = []
    for d in a.dirs:
        for js in sorted(d.glob("*.json")):
            m = json.loads(js.read_text())
            iq = np.load(js.with_suffix(".npy"))
            for s, e, fc, snr in m["bursts"]:
                car = m["center_mhz"] + fc / 1e6
                if (e - s) / FS * 1e6 < a.min_us or abs(car - a.mhz) > 1.5:
                    continue
                r = frame_bits(iq, s, e, m["center_mhz"], a.mhz)
                if r is None or not (300 < r["dev_khz"] < 700):
                    continue
                post = "".join(map(str, r["post"][: a.nbits]))
                rows.append(dict(cap=js.stem, s=int(s), t=m["time"], us=(e - s) / FS * 1e6,
                                 pre=r["pre_len"], f0=r["f0_khz"], dev=r["dev_khz"], post=post))
    rows.sort(key=lambda r: (r["t"], r["s"]))
    for r in rows:
        print(f"{r['cap']:30s} {r['us']:5.1f}us pre={r['pre']:3d} f0={r['f0']:+5.0f} dev={r['dev']:3.0f} {r['post']}")
    # common prefixes after preamble
    for L in (16, 24, 32, 40, 48):
        c = Counter(r["post"][:L] for r in rows if len(r["post"]) >= L)
        print(f"top post-preamble {L}-bit prefixes:", [(k, v) for k, v in c.most_common(4)])
    if a.out:
        a.out.write_text(json.dumps(rows))
    return 0


if __name__ == "__main__":
    sys.exit(main())
