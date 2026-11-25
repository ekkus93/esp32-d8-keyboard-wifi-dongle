# ESP32-S3 transmit path — lab results

Status: **TX modulator verified between the project's two boards (2026-11-25).** No transmission to the keyboard has been made. Lab build only (`main/lab_main.c`), per docs/DECISIONS.md.

## Setup

- **Board B** (TX): lab firmware from this repo. `components/phy_s3` (Halo bridge 67deba5, runtime carrier + settle parameter), `components/gfsk` (Halo modulator adapted to 2 Mbit/s with runtime deviation, digital IF and chirp), `components/esb` (own frame builder).
- **Board A** (RX monitor): ESP-SDR `3ab3b65`, `CAP16` 16 MS/s snapshots at 2440 MHz.
- Test frame: a copy of the keyboard's search request (RF_PROTOCOL.md D2) on the pairing access code, transmitted at 2443 MHz, gain index 4, 1 ms between sends.
- Harness: `tools/lab/txtest.py` (runs both boards, locates frames with `esb_match`, slices with a carrier fitted against the known bits, reports bit errors, deviation, drift and carrier). Unit test: `tests/unit/test_esb.c` (builder reproduces both captured keyboard frames incl. CRC).

## Results

| Run | Settle | Deviation set | IF | Chirp | Board A gain | Frames | Exact | Measured dev | In-frame drift |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 3000 µs | 500 kHz | 0 | 0 | AGC | 9 | 3 | 342 kHz | −98 kHz |
| 2 | 300 µs | 650 kHz | +3 MHz | 0 | AGC | 24 | 3–9 (varied by run) | ~500 kHz | −63…−92 kHz |
| 3 | 300 µs | 800 kHz | +3 MHz | +1700 Hz/µs | AGC | 24 | 5 | 596 kHz | −2 kHz |
| 4 | 300 µs | 800 kHz | +3 MHz | +1700 Hz/µs | **manual 20 / 35 / 50** | 24 / 34 / 20 | **24 / 34 / 20** | 535–590 kHz | −36…−64 kHz |
| 5 | 300 µs | 500 kHz | 0 | 0 | manual 35 | 21 | **21** | 349 kHz | −100 kHz |
| 6 | 100 µs | 650 kHz | +3 MHz | 0 | manual 35 | 31 | **31** | 443 kHz | −127 kHz |

Keyboard reference with the same measurement (`cap_tap3`, AGC, keyboard ~1 m away): 163/167 exact; measured deviation ≈ 389 kHz, drift ≈ −65 kHz.

## Findings

1. Frames from board B decode on board A with **zero bit errors** once board A's gain is fixed (runs 4–6: 106/106). Format, bit rate, CRC and carrier are correct.
2. The residual 1–3 bit errors in runs 1–3 (clustered at bits 17–26 of the access code) were caused by **board A's hardware AGC** reacting to the strong close-range signal, not by the transmitter. Moving the frame in time (lead 400 vs 2400 samples) did not move the errors; fixing the gain removed them.
3. Board B leaks its LO as a strong unmodulated carrier while in TX mode (≈ +14 dB over the noise in averaged spectra, also during the settle period). A digital IF (LO tuned to channel − IF) moves this leakage out of the channel.
4. TX settle can be reduced from Halo's 3000 µs to 100–300 µs without measurable quality loss.
5. Carrier drift within a frame is ≈ −100 kHz without compensation; a +1700 Hz/µs chirp flattens it.
6. Carrier accuracy board B → board A: about −70…−100 kHz at 2443 MHz (combined crystal error of both boards).

## Open items

- Absolute frequency calibration of ESP-SDR's 40 MS/s `RINGCAP` mode (RF_PROTOCOL.md T2).
- TX gain table is uncalibrated (Halo: "upstream only ever tested 8"); output power not measured.
- Recommended starting configuration for further work: settle 300 µs, deviation 650 kHz, IF +3 MHz, chirp +1700 Hz/µs.
