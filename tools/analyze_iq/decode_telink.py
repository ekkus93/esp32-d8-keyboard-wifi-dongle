#!/usr/bin/env python3
"""Decode Telink-style frames from saved captures (layout per docs/RF_PROTOCOL.md D1):
find the access-code tail, then 9-bit header, payload (MSB-first bytes), 16-bit trailer."""
import json, subprocess, sys
from collections import Counter
from pathlib import Path

HERE = Path(__file__).parent
TAIL = "0101010101100011100"  # end of access code ('…AA AA 71' on air) – loose anchor
ANCHOR = "10001110"            # 0x71 LSB-first, last address byte

def frames(d: Path, tmp: Path):
    rows, seen = [], set()
    for mhz in [2405 + 1.0 * i for i in range(75)]:
        subprocess.run([sys.executable, HERE / "gfsk_frames.py", str(d), "--mhz", str(mhz),
                        "--nbits", "240", "--out", str(tmp)], capture_output=True)
        try:
            data = json.loads(tmp.read_text())
        except Exception:
            continue
        for x in data:
            k = (x["cap"], x["s"])
            if k in seen:
                continue
            seen.add(k); x["mhz"] = mhz; rows.append(x)
    return rows

def decode(q: str):
    i = q.find(ANCHOR, 20, 60)
    if i < 0:
        return None
    a = i + 8
    hdr = q[a:a + 9]
    if len(hdr) < 9:
        return None
    L = int(hdr[:6], 2)
    pay = q[a + 9:a + 9 + 8 * L]
    trl = q[a + 9 + 8 * L:a + 9 + 8 * L + 16]
    if len(pay) < 8 * L or len(trl) < 16:
        return dict(L=L, pid=int(hdr[6:8], 2), na=hdr[8], pay=None, trl=None, addr_end=a)
    return dict(L=L, pid=int(hdr[6:8], 2), na=hdr[8], addr_end=a,
                pay=" ".join("%02x" % int(pay[j:j + 8], 2) for j in range(0, 8 * L, 8)),
                trl="%04x" % int(trl, 2))

def main():
    d = Path(sys.argv[1]); tmp = Path(sys.argv[2])
    rows = frames(d, tmp)
    c = Counter()
    for x in sorted(rows, key=lambda r: (r["t"], r["s"])):
        r = decode(x["post"])
        if not r:
            continue
        key = (r["L"], r["pay"], r["trl"])
        c[key] += 1
        print(f"{x['cap']:32s} {x['mhz']:.0f} {x['us']:5.1f}us L={r['L']:2d} pid={r['pid']} na={r['na']} {r['pay']} | {r['trl']}")
    print("\n== distinct (L, payload, trailer):")
    for k, v in c.most_common(30):
        print(v, k)

main()
