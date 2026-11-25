# D8 RF protocol — evidence log

Status: **early characterization (TODO P3.2–P3.3, P4.3 in progress).** Nothing here is a verified protocol. Confidence: H = repeated, controlled; M = repeated, not fully controlled; L = single observation or inference.

## Setup (2026-11-25)

- Receiver: ESP32-S3 rev v0.2 (N16R8), ESP-SDR firmware rev `3ab3b65` (external, GPL-3.0, unmodified; built against ESP-IDF `25fe69f9`, image SHA-256 `3d2b3e24…0202`), RX only.
- Acquisition: `CAP16 16380 6` snapshots (16 MS/s, 1.02 ms each, gaps between snapshots), hardware AGC, centres 2412–2476 MHz. Host tools: `tools/capture/scan.py`, `tools/analyze_iq/*.py`.
- Keyboard: owner's D8 (TLSR8368), no dongle present, ~1 m from receiver. Test input: single key `a` tapped ~1/s by owner.
- Captures are kept outside the repo (session scratchpad), not committed.

## Observations

| # | Finding | Conf. | Evidence |
|---|---|---|---|
| O1 | Keyboard-off control: 0 bursts with 2 Mbps-GFSK signature in 1800 snapshots at centres 2449/2470/2475 MHz | H | run `kb_off2` |
| O2 | Keyboard on + tapping: 168 such bursts / 1800 snapshots (centres 2449/2470); keyboard on idle: 5 / 500 (centre 2475) | H | runs `kb_on_tap2`, `kb_on_idle_2475` |
| O3 | Dominant carrier ≈ **2475.8–2476.0 MHz** (observed from tuning centres 2470, 2474, 2475). Slightly above the FCC grant's 2474 MHz upper limit; cause unknown (possibly measurement offset) | M | runs `kb_on_tap2`, `kb_poweron_tap_2474` |
| O4 | Weaker candidate clusters ≈ 2443.75 and 2455.6 MHz; not yet controlled (2455.6 is at a capture edge) | L | run `kb_on_tap2` |
| O5 | Modulation: 2-level FSK/GFSK, deviation ≈ ±450–600 kHz, **2 Mbps** (≈8 samples/bit at 16 MS/s); consistent with TLSR8368 2 Mbps mode | H | per-bit FM traces, `gfsk_frames.py` |
| O6 | Each burst begins with ~10 µs of carrier settling, then a long alternating preamble (~70–90 bits) | M | FM traces |
| O7 | Two lengths: ~58 µs and ~108 µs. Both share the post-preamble bit sequence `1011 0010101010101010101011 000111000101100101010001 0100` | H (≥40 frames) | `gfsk_frames.py` prefix histogram |
| O8 | Long frames continue with mostly-zero bits, e.g. `…00000001 0000…0000 1100 0000…0001 0000001100001101011100` then ~8–16 varying bits. Suggests no whitening; varying tail may be CRC/sequence | L | 14 long frames, 9 distinct (some differences are likely bit errors) |
| O9 | Keyboard transmits occasionally while idle with no dongle present | M | run `kb_on_idle_2475` |
| O10 | After the keyboard had been idle for a while, 30 s untouched produced 0 bursts (O9 idle traffic was shortly after tapping) | M | run `t1_idle` |
| O11 | The same frame signature (O7) appears on multiple carriers: ≈2429.0, 2453.5, 2459.75, 2465.6, 2476.0 MHz (±~0.3 MHz estimate). Keyboard uses several channels | H | run `t3b_tap_down_wide` |
| O12 | Long-frame consensus (first ~145 bits after preamble) is **identical** for tapping `a` (11 frames), tapping Down-arrow and holding Down-arrow (21 frames). Only the last ~6 bits vary, not by key. Holding a key does not produce continuous traffic (11 bursts / 30 s) | H | runs `kb_on_tap2`, `kb_poweron_tap_2474`, `t2_hold_down`, `t3_tap_down`, `t3b_tap_down_wide` |

### Interpretation (inference, not verified)

The observed frames do not encode which key was pressed. Most consistent explanation: with its receiver absent, the keyboard wakes on a key event and repeats a link/pairing-search request across several channels, waiting for a dongle response that never arrives; key reports would be sent only after a link is established. If correct, **RX-only operation is insufficient** (SPEC §6.3, TODO P6.5): a receiver would need to send the dongle's response, which requires the gated lab-response path and owner approval.

## Not established

Sync/access-address boundary, byte alignment, length field, CRC algorithm (16-bit CCITT/IBM/DNP/T10 brute force: no hit distinguishable from chance), payload mapping, press vs release frames, channel hopping, retransmit timing, whether an ACK/dongle response is expected.

## Next steps

1. Gap-free `RINGCAP 3 6` captures at 2476 MHz to see complete frames and retransmit intervals.
2. Labeled trials: press-and-hold vs release, different keys (`a`, `b`, Shift), touchpad motion — compare differing bits.
3. Improve demod (matched filter, timing recovery, carrier from preamble) to reduce bit errors before CRC search.
4. Controlled check of the 2443.75 / 2455.6 MHz clusters for hopping.
