# Project status

**Updated:** 2026-11-25 · **Base commit:** `8bcdc4d` (this update adds the TX-quality work on top)

## Current gate

- **G0 (baseline):** partially met — ESP-SDR RX reproduced on both boards; lab TX firmware builds and runs. Official TinyUSB HID example not yet run (P5.1).
- **G1 (D8 RF):** met — repeatable keyboard-attributable bursts with on/off controls, no dongle present (RF_PROTOCOL.md O1–O12).
- **G2 (protocol):** met for the keyboard's link-search frames — framing, CRC and payload verified on 217/217 clean frames (D1–D3, D8). Key-report frames not yet observed (they are only sent after a link exists).
- **G3 (link):** not met. Requires a dongle response; no transmission to the keyboard has been made.

## Completed since start

| Item | Evidence |
|---|---|
| Hardware identification (TLSR8368, P25Q40H, both S3 boards) | docs/HARDWARE.md |
| Band survey, attribution, modulation (2 Mbps GFSK) | docs/RF_PROTOCOL.md O1–O12 |
| Frame format + CRC (nRF ESB CRC-16) | RF_PROTOCOL.md D1–D3, `tools/analyze_iq/decode_telink.py` |
| Search-attempt timing (531 µs) | RF_PROTOCOL.md T1, `tools/analyze_iq/esb_match.py` |
| License → GPL-3.0-only; owner decisions | docs/DECISIONS.md, docs/THIRD_PARTY.md |
| S3 lab TX firmware, ESB builder + host test | `main/`, `components/`, `tests/unit/test_esb.c` |
| TX path verified board B → board A, 0 bit errors | docs/S3_TX.md |

## Blockers

| Item | Cause | Action |
|---|---|---|
| Link with keyboard (G3) | The dongle-response experiment was halted by an automated safety check on 2026-11-25 | Owner to decide how to proceed |
| ESP-SDR 40 MS/s frequency offset (T2) | Uncalibrated mode | Calibrate with a known tone from board B |

## Next actions (receive-only / independent)

1. On-chip gap-free RX with frame detection and timestamps in the lab firmware (P2.4, P4.10).
2. USB HID path: official TinyUSB example, descriptors and input state machine with host tests (P5.1–P5.6).
3. Host CTest/pytest wiring and CI (P0.7).

## Owner assistance required

- Decision on how to continue the link (G3) work.
- Later: key taps for receive-side tests when requested.
