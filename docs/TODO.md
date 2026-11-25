# ESP32-S3 D8 Receiver — Implementation TODO

**Date:** 2026-11-25 | **Spec:** [SPEC.md](SPEC.md) | **Status:** Proposed; no implementation or hardware verification completed.

## Execution rules

- Work in dependency order where necessary. Prefer independent software/CI work while physical tests are blocked. Do not fabricate captures, protocol findings or hardware results.
- Check a task only after its deliverable and acceptance evidence exist. Keep blocked tasks unchecked, annotate the blocker and required action. Close a conditional task as not applicable only with evidence and rationale; do not silently count it as implemented.
- Maintain `docs/STATUS.md` with exact commit, current gate, evidence paths, blockers, next action and owner assistance required. Every phase has a hardware or software acceptance gate; mocks cannot satisfy physical tests.
- Default to RX-only experimental firmware and explicit enrollment of the owner's keyboard. No unidentified transmissions or automatic HID injection. Follow the spec's compliance, privacy and recovery requirements.
- If a repository and CI workflow are later provided, use Ralph Bridge exclusively for GitHub/CI operations, work on `master` with exact-head compare-and-swap where authorized, and verify CI against the actual committed head. No per-task branch/PR requirement. Do not assume repository creation or push permission from this file.

## P0 — Project scaffold and reproducible build (software-first)

- [ ] P0.1 Initialize ESP-IDF project targeting `esp32s3`; create component directories, README, build prerequisites and `.gitignore`.
- [ ] P0.2 Record target board requirements (N16R8 flash/PSRAM, native USB, UART, boot recovery) in `docs/HARDWARE.md`.
- [ ] P0.3 Verify availability of proposed ESP-IDF commit `25fe69f946311abdaf9ad56591f25fedbc20ac98`; pin SDK, submodules, compiler and components; document stable SDK comparison separately.
- [ ] P0.4 Audit ESP-SDR, Halo bridge and TinyUSB source/license requirements; write `docs/THIRD_PARTY.md`, notices and project license decision before incorporating code.
- [ ] P0.5 Establish `radio_hal` API, capability structure, lifecycle, error codes, null/fake backend and build-time target guards.
- [ ] P0.6 Configure RX diagnostic, USB HID test and normal receiver build profiles. Experimental response functionality disabled by default.
- [ ] P0.7 Configure CI for pinned ESP-IDF builds, host CTest, pytest, lint, sanitizers and build artifact/checksum publication.
- [ ] P0.8 Document build, flash, monitor, erase/recover commands; ensure safe boot mode and UART alternative.
- [ ] P0.9 Create `docs/RF_PROTOCOL.md`, `docs/TEST_PLAN.md`, `docs/STATUS.md` and a hardware evidence template; label all unknowns explicitly.

**Acceptance:** Clean checkout builds RX-only and USB test profiles; host CI passes; no hardware claims. Record build size and SDK SHA.

## P1 — Hardware identification and normal connection checks (owner-assisted)

- [ ] P1.1 Obtain actual S3 board model, native USB port, silicon revision, flash/PSRAM detection and serial output.
- [ ] P1.2 Photograph D8 label, battery compartment, and PCB markings if needed (battery disconnected before opening).
- [ ] P1.3 Compare FCC ID 2AQRX-D8 exhibits and manual with actual revision; record matching and conflicting evidence.
- [ ] P1.4 Identify RF/MCU chipset if markings permit; locate datasheets and normal pairing instructions specific to revision.
- [ ] P1.5 Test whether normal Bluetooth discovery or wired USB HID is available; record exact observations and simpler workaround if found.
- [ ] P1.6 Establish isolated, repeatable bench setup with charged keyboard, known USB cable, UART recovery and safe test host.

**Acceptance:** Hardware identity documented as far as evidence permits. Unknowns retained, not guessed. If ordinary Bluetooth/wired mode solves the owner's problem, ask whether to continue proprietary RF research.

## P2 — Reproduce S3 undocumented RF RX baseline

- [ ] P2.1 Review upstream ESP-SDR implementation and applicable S3 capture mode, including internal-memory, timing and private PHY dependencies.
- [ ] P2.2 Reproduce upstream RX demonstration on the pinned supported build; save revision, setup, output and any failure logs.
- [ ] P2.3 Port or implement the RX-only `phy_s3` adapter within license constraints; verify target and private symbols, explicit initialization/cleanup and deterministic error paths.
- [ ] P2.4 Implement bounded I/Q capture with sample metadata, discontinuity flags, sequence counters, timeouts and error reporting.
- [ ] P2.5 Benchmark snapshots and ring acquisition: rate, sample format, bandwidth, gain, capture duration, lost samples, CPU, SRAM and PSRAM.
- [ ] P2.6 Validate receive baseline against a known authorized source; measure frequency accuracy, noise and gain behavior; preserve traces.
- [ ] P2.7 Provide a capture export utility with a versioned format and metadata/hash manifest; validate truncation and transport corruption detection.
- [ ] P2.8 Verify repeatable start/stop, watchdog recovery and USB/UART transport behavior. Test on actual N16R8 board.
- [ ] P2.9 Confirm experimental RF transmission is absent/disabled in the RX build.

**Gate G0 (joint with P5):** RX baseline and separate official HID demo work on actual hardware; CI cannot replace bench evidence.

## P3 — D8 signal characterization and evidence collection

- [ ] P3.1 Prepare controlled experiment script: keyboard off, idle, power-on, wake, known labeled press/release and touchpad movement; exclude unrelated radio traffic.
- [ ] P3.2 Survey candidate 2408–2474 MHz band with repeatable controls; log occupancy and capture gaps, compare off/on and action correlations.
- [ ] P3.3 Verify candidate D8 emissions in multiple independent trials and power cycles; distinguish bursts from unrelated traffic.
- [ ] P3.4 Determine whether useful transmissions occur without a receiver and whether normal enrollment is needed. If no attributable signal, inspect revision, battery, acquisition limits and normal pairing procedure before concluding.
- [ ] P3.5 Establish sufficient sample rate and bandwidth for candidate signals; use high-rate snapshots/on-device detection if decimated streaming cannot preserve symbols.
- [ ] P3.6 Generate authorized labeled capture corpus and holdout split; include positive, negative, idle, release, long hold, repeated and mixed actions.
- [ ] P3.7 Record all results in `docs/RF_PROTOCOL.md`, including uncertainty, trace IDs and reproducibility steps; keep private/unrelated captures out of repository.

**Gate G1:** Repeatable evidence of D8 transmissions without a receiver, or evidence of a legitimate enrollment path. If unavailable, request owner/reference hardware assistance; continue independent software tasks.

## P4 — Software modem, framing and input protocol

- [ ] P4.1 Create offline Python capture loader, metadata validator, spectrum/waterfall and burst inspection tools; test against synthetic data.
- [ ] P4.2 Implement portable candidate modem interfaces and synthetic DSP fixtures; select modulation and symbol recovery only after evidence. GFSK is a hypothesis, not a requirement.
- [ ] P4.3 Characterize observed symbol timing, modulation and channel behavior; compare competing hypotheses and retain measured confidence.
- [ ] P4.4 Establish frame boundaries, length, bit/byte ordering and integrity checks (including CRC/whitening if applicable); validate against independent labeled samples.
- [ ] P4.5 Implement bounded portable frame parser with explicit rejection reasons; unit tests, sanitizer and fuzz tests for malformed/truncated data.
- [ ] P4.6 Identify verified action mappings for keyboard press/release, modifiers, buttons, relative movement, scrolling and consumer controls.
- [ ] P4.7 Establish sequence/duplicate behavior and supported hardware profile identity from evidence; do not assume ESB or nRF24 compatibility.
- [ ] P4.8 Implement versioned D8 decoder profile and deterministic replay harness; test held-out fixtures and report decode/action accuracy.
- [ ] P4.9 Document unsupported fields/actions and conditions where input must be rejected. Avoid interpreting unverified fields as commands.
- [ ] P4.10 Benchmark portable decoder on S3; verify processing latency, ring gaps, CPU and memory. Optimize only against measured bottlenecks.

**Gate G2:** Independent captures validate framing, integrity and action mappings; synthetic fixtures alone insufficient. If association is needed first, revisit P6 and then re-run G2.

## P5 — USB HID and input state (independent of RF research)

- [ ] P5.1 Build official TinyUSB keyboard/mouse example on proposed ESP-IDF version; confirm target hardware enumerates via native USB OTG.
- [ ] P5.2 Implement keyboard, relative mouse and consumer-control HID descriptors with legitimate USB identity and documented report format.
- [ ] P5.3 Implement portable input state machine, modifiers, press/release, supported rollover, mouse button state and bounded relative motion accumulation.
- [ ] P5.4 Add HID scheduling, endpoint readiness handling, queue limits, media key release and fail-safe all-release path; ensure no stale actions after reconnect.
- [ ] P5.5 Implement USB suspend/resume, unplug/replug and reset behavior; ensure Serial/JTAG vs OTG PHY conflict is handled with UART diagnostics or separate modes.
- [ ] P5.6 Add host tests with fake endpoint and synthetic events: long holds, repeated frames, combinations, motion saturation, backpressure, overflow and release behavior.
- [ ] P5.7 Provide physically activated test-only HID demonstration, disabled in normal builds; verify on isolated Linux host using `lsusb`, `evtest` and kernel logs.
- [ ] P5.8 Document host compatibility and descriptor validation results; optional Windows test.

**Acceptance:** USB and state machine complete independently of RF. Record physical HID results for G0. No synthetic test mode automatically emits input at boot.

## P6 — Association, channel management and link feasibility

- [ ] P6.1 Document verified normal association/enrollment flow, peer identity and whether bidirectional traffic is necessary; require owner action or reference dongle if justified.
- [ ] P6.2 Implement explicit user-controlled association and peer filtering; reject unknown or ambiguous peers, unsupported profiles and invalid frames.
- [ ] P6.3 Implement versioned NVS persistence, safe reset, corrupted record recovery and reconnect for known keyboard.
- [ ] P6.4 Implement channel tracking/retry/duplicate logic only where observed; handle timeouts and loss without interpreting unrelated traffic.
- [ ] P6.5 Determine whether RX-only operation suffices. Record evidence; close P6.6–P6.8 as not applicable only if no RF response is needed.
- [ ] P6.6 If responses are required, evaluate internal S3 radio capability, timing, signal quality, license and regulatory constraints; use separate disabled-by-default lab build and controlled tests. No guessed or unverified on-air responses.
- [ ] P6.7 If feasible and authorized, implement only verified normal link responses and test timing, interoperability and safe shutdown in compliant conditions.
- [ ] P6.8 If S3 internal radio cannot satisfy the verified protocol, document measurements and compare compatible external transceiver options. Obtain owner approval before changing the S3-only scope or purchasing hardware.
- [ ] P6.9 Test power-cycle, sleep/wake, enrollment reset, unknown peer rejection and association restoration on hardware; no authentication bypass.

**Gate G3:** Legitimate association and sustained usable input traffic from the owner’s D8. If unavailable, no working receiver claim; software work can continue.

## P7 — Integrate end-to-end receiver

- [ ] P7.1 Connect capture → modem → validated framing → selected peer → input events → HID using bounded queues and explicit state transitions.
- [ ] P7.2 Implement timestamping, performance counters, discontinuity recovery and queue high-water metrics without logging production keystrokes.
- [ ] P7.3 Define link-loss detection from observed protocol and test fail-safe release, held-key semantics and resynchronization.
- [ ] P7.4 Implement diagnostics mode separation, capability and status reporting, safe boot and configuration reset; ensure no debug transmission or raw HID injection in normal build.
- [ ] P7.5 Run end-to-end simulated replay with fake radio and USB, asserting exact expected report sequences and zero unexpected actions.
- [ ] P7.6 On target hardware, enumerate receiver and type controlled test input from D8 with no original dongle and no host daemon.
- [ ] P7.7 Verify actual keyboard, modifiers, navigation, multimedia, touchpad, clicks, scrolling and long holds against supported action matrix.
- [ ] P7.8 Test recovery from RF loss, USB reconnect, reset, sleep, low battery and queue pressure; preserve evidence and fix regressions.

**Gate G4:** Live D8 input reaches OS via ESP32-S3 native USB HID, without host daemon. Record exact board, firmware SHA, keyboard revision and observed unsupported functions.

## P8 — Hardening and release verification

- [ ] P8.1 Measure p95/p99 physical-input-to-host latency with documented timing method; compare against SPEC targets.
- [ ] P8.2 Run ≥1,000 mixed labeled transitions; calculate fidelity and verify zero unintended actions/stuck keys.
- [ ] P8.3 Run ≥1-hour mixed-input/idle stability test; record memory/stack, queue, capture loss and reset counters.
- [ ] P8.4 Execute ≥20 power-cycle/reconnect trials; verify persistent association and fail-safe state.
- [ ] P8.5 Verify link loss releases, valid long holds, endpoint busy, parser robustness, unknown traffic rejection and default no-transmit diagnostics.
- [ ] P8.6 Verify Debian and Ubuntu built-in HID support; record Windows compatibility if tested and Linux fallback if any.
- [ ] P8.7 Review third-party licenses, RF compliance, USB identity, privacy and distribution restrictions; update notices.
- [ ] P8.8 Complete README with supported revision, flashing, pairing, recovery, limitations, troubleshooting and optional enclosure/connector guidance.
- [ ] P8.9 Run exact-head CI for all supported profiles; publish build outputs, checksums, test reports and reproducibility metadata.
- [ ] P8.10 Review all acceptance criteria and unresolved blockers; update spec with measured parameters and release notes. No release-ready label without required hardware evidence.

**Gate G5:** All mandatory requirements satisfied and measured targets met or explicitly renegotiated; known limitations and supported revision documented.

## Owner assistance checklist (when required)

1. Provide S3 board model and D8 PCB/label photos if software research cannot identify revision.
2. Flash diagnostic firmware and run guided bench captures with known test actions; provide exported artifacts and logs.
3. Test USB HID enumeration and controlled input on Linux. A reference dongle or compatible radio may be requested only if evidence justifies it.
4. Approve any scope change to external hardware or relaxed acceptance criteria. The original S3-only approach remains primary.

## Initial blockers and next action

| Item | Status |
|---|---|
| Project repo/CI | Not provided; scaffold can be prepared locally |
| Exact D8 revision/chipset | Unknown; P1 |
| Local S3 SDR validation | Pending physical board; P2 |
| D8 waveform/protocol | Unknown; P3–P4 |
| Need for bidirectional RF and viability | Unknown; P6 |
| Physical HID test | Pending physical board; P5 |

**Next autonomous task:** P0.1–P0.9, then portable analysis and USB state-machine testing in parallel with bench-dependent work. Never mark a blocked bench item completed on the basis of CI alone.
