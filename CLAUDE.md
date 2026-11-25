# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project state

Replacement USB receiver for a D8 mini wireless keyboard, built on an ESP32-S3-N16R8 using undocumented radio I/Q access and native USB HID. The spec and plan are the source of truth:

- @docs/SPEC.md — requirements, architecture, feasibility gates G0–G5
- @docs/TODO.md — phased tasks P0–P8 and execution rules

At the time of writing the repo is docs-only; the ESP-IDF scaffold (P0) does not exist yet.

## Toolchain

- Activate ESP-IDF in each shell before `idf.py`: `. ~/esp/esp-idf/export.sh` (not on PATH by default; may not be installed yet — see TODO P0.3).
- Target SDK is the pinned ESP-IDF commit `25fe69f946311abdaf9ad56591f25fedbc20ac98`; do not assume stable 6.1 or another release is compatible.
- Host tests (CTest, pytest) must run without ESP32 hardware.
- P0 should also add linting/formatting (clang-format + clang-tidy for C/C++, ruff for Python tools) and then a format-on-edit hook in `.claude/settings.json`.

## Git

- Commit directly to `master`. Never push without asking first.
- Project license is GPL-3.0-only (changed from Apache-2.0 on 2026-11-25 so ESP-SDR / Halo bridge code can be reused). Keep upstream copyright/SPDX headers on reused files and list them in `docs/THIRD_PARTY.md`. Never copy code from unlicensed sources (e.g. the Telink KM SDK in `~/esp/ref/telink_km`) — use them as protocol reference only.
- Owner approved RF transmission experiments on 2026-11-25 (lab build only, low power, aimed at the owner's keyboard). Normal receiver builds still must not transmit anything beyond the verified link responses.

## Rules that are easy to break

- Never fabricate captures, protocol findings, or hardware results. Mocks, CI, or a passing build never satisfy a hardware gate or a physical test.
- Check a TODO box only when its deliverable and evidence exist; annotate blocked tasks instead. Keep `docs/STATUS.md` current (use `/status-update`).
- D8 modulation/protocol is unknown: do not treat GFSK, ESB, nRF24 compatibility, or any pairing sequence as established. Record conclusions with confidence and trace IDs in `docs/RF_PROTOCOL.md`.
- Firmware is RX-only by default. Any RF transmission lives only in a separate, disabled-by-default lab build and only after evidence shows a response is required.
- All private PHY/register/ROM dependencies stay inside `phy_s3` behind `radio_hal`; a public-SDK-only HID build must remain possible.
- GPIO19/20 are native USB D−/D+; never reassign them in HID builds. USB Serial/JTAG and USB OTG share the PHY — use UART for diagnostics in HID builds.
- No HID output from RF energy, guessed payloads, or unenrolled peers; nothing emits HID input automatically at boot. Release all held input on link loss.
- Never log raw keystrokes in normal/production mode. Never commit raw RF captures of unrelated traffic or personal input.
