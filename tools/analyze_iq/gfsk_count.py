#!/usr/bin/env python3
"""Count bursts with a 2 Mbps GFSK signature (median FM run ~8 samples @16 MS/s,
deviation 350-800 kHz) in saved captures; report absolute carrier estimates."""
import json, sys
from pathlib import Path
import numpy as np
sys.path.insert(0, str(Path(__file__).parent))
from inspect_bursts import analyze, FS

def main():
    d = Path(sys.argv[1]); min_us = float(sys.argv[2]) if len(sys.argv) > 2 else 15
    hits, ncap = [], 0
    for js in sorted(d.glob("*.json")):
        m = json.loads(js.read_text()); iq = np.load(js.with_suffix(".npy")); ncap += 1
        for s, e, fc, snr in m["bursts"]:
            if (e - s) / FS * 1e6 < min_us: continue
            r = analyze(iq, s, e, fc)
            if r is None or not len(r["runs"]): continue
            mr = np.median(r["runs"])
            if 6 <= mr <= 12 and 350 <= r["dev_khz"] <= 800:
                hits.append((js.stem, m["center_mhz"] + (fc + r["f0_khz"] * 1e3) / 1e6, r["len_us"], snr, m["time"]))
    print(f"saved_caps={ncap} gfsk2m_bursts={len(hits)}")
    if hits:
        f = np.array([h[1] for h in hits]); L = np.array([h[2] for h in hits])
        print(f"carrier MHz: median={np.median(f):.3f} p10={np.percentile(f,10):.3f} p90={np.percentile(f,90):.3f}")
        print(f"len us: median={np.median(L):.1f} max={L.max():.1f}")
        print("hist carrier (0.25 MHz):", dict(zip(*np.unique(np.round(f*4)/4, return_counts=True))))
main()
