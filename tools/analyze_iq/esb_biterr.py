#!/usr/bin/env python3
"""Bit-error count of known D8 frames in 16 MS/s captures (CAP16 .npy files or a stacked .npy).

Locates each frame with the esb_match matched filter, then slices it using a
carrier offset fitted against the known bit pattern (no moving-average filter,
so long runs of equal bits survive). 0 errors over all 153 bits = an exact,
CRC-valid frame.
"""
import glob
import sys
from pathlib import Path

import numpy as np
from scipy.signal import firwin, lfilter

sys.path.insert(0, str(Path(__file__).parent))
import esb_match as M  # noqa: E402

FS = 16e6


def biterrs(z, center, span=6.0, thresh=0.5, frame="search"):
    ref = M.FRAMES[frame]
    tb = np.array([1.0 if b == "1" else -1.0 for b in ref])
    out = []
    for h in M.scan(z, FS, center, span, 0.25, thresh):
        if h["frame"] != frame:
            continue
        n = np.arange(len(z))
        bb = lfilter(firwin(63, 1.4e6, fs=FS), 1, z * np.exp(-2j * np.pi * (h["mhz"] - center) * 1e6 / FS * n))
        f = np.angle(bb[1:] * np.conj(bb[:-1]))
        hp = f - np.convolve(f, np.ones(64) / 64, "same")
        k = int(np.argmax(np.correlate(hp, M.TEMPL[frame], "valid")))
        best = None
        for d in range(-8, 9):
            idx = k + d + 4 + 8 * np.arange(len(ref))
            if idx[0] < 0 or idx[-1] >= len(f):
                continue
            s = f[idx]
            a, b = np.polyfit(tb, s, 1)          # s ≈ a*bit + b: b = carrier offset
            bits = (s - b) > 0
            err = int(np.sum(bits != (tb > 0)))
            if best is None or err < best[0]:
                best = (err, h["mhz"], h["score"])
        if best:
            out.append(best)
    return out


def main():
    src, center = sys.argv[1], float(sys.argv[2])
    if src.endswith(".npy") and Path(src).is_file():
        caps = list(np.load(src))
    else:
        caps = [np.load(f) for f in sorted(glob.glob(str(Path(src) / "*.npy")))]
    allr = []
    for z in caps:
        allr += biterrs(z, center)
    errs = sorted(r[0] for r in allr)
    print(f"{len(allr)} frames; bit errors: {errs}; exact: {sum(e == 0 for e in errs)}")


if __name__ == "__main__":
    main()
