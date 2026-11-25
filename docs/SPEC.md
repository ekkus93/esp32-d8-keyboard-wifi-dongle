# ESP32-S3 D8 Wireless Keyboard Receiver — Technical Specification

**Project:** `esp32-d8-receiver`  
**Revision:** 0.1 — research and proof of concept  
**Date:** 2026-11-25  
**Companion:** [TODO.md](TODO.md)

## 1. Purpose

Build an open-source replacement USB receiver for the owner's D8 mini wireless keyboard and touchpad, whose original USB receiver is missing. The primary approach is to use an ESP32-S3 alone, exploiting experimental, undocumented radio I/Q access to receive the keyboard's proprietary 2.4 GHz protocol and translating validated input into standard USB HID reports.

The keyboard should remain unmodified. The finished receiver should require neither a Linux daemon nor an additional radio IC. A compatible external radio is a contingency, not a requirement for the first implementation.

This is a feasibility-gated engineering project. No D8 RF capture, chipset identification, packet format, or successful pairing has been established on the owner's unit. USB HID is supported by Esp-IDF; proprietary RF compatibility is not guaranteed.

## 2. Requirements and scope

| ID | Requirement | Priority |
|---|---|---|
| R01 | Build for ESP32-S3-N16R8 (16 MB flash, 8 MB PSRAM), using ESP-IDF and C/C++ | Must |
| R02 | Isolate all private PHY dependencies behind an explicit radio abstraction | Must |
| R03 | Receive and characterize authorized D8 radio activity using the S3's built-in radio | Must investigate |
| R04 | Identify the exact D8 hardware revision and establish the on-air protocol from reproducible evidence | Must |
| R05 | Implement receiver association, acknowledgments and channel behavior only as required by the verified protocol | Conditional |
| R06 | Present standard USB keyboard, relative mouse and consumer-control HID reports via native USB OTG | Must |
| R07 | Support ordinary keys, modifiers, releases, mouse motion, clicks, and scrolling where the keyboard generates these events | Must |
| R08 | Support normal media and navigation keys when observable; local-only lighting/DPI functions need not generate HID reports | Should |
| R09 | Persist the selected keyboard association and recover after reset, disconnect and sleep | Must |
| R10 | Fail closed on unknown, corrupt, unassociated, ambiguous or unsupported traffic; release held input on link failure | Must |
| R11 | Provide offline analysis tools, synthetic test fixtures, replay, automated tests and reproducible firmware builds | Must |
| R12 | Operate without Wi-Fi, cloud services or a host daemon in normal receiver mode | Must |
| R13 | Provide controlled diagnostics and a recoverable flashing path | Must |

### Non-goals

- Universal wireless keyboard compatibility, interception of other people's input, or bypassing pairing/authentication.
- Arbitrary RF waveform transmission, high-power operation, jamming or network coexistence attacks.
- A general-purpose SDR application, GUI or mobile app for the initial deliverable.
- Claiming compatibility with all products sold as D8; support is revision-specific until tested.
- Reimplementing the original dongle's exact USB VID/PID or proprietary branding.

## 3. Evidence and assumptions

| Item | Status |
|---|---|
| D8 mini wireless keyboard/touchpad with missing dongle | Confirmed by owner and photographs |
| FCC ID 2AQRX-D8 lists 2408–2474 MHz | Documented for a D8 model; owner's unit not yet matched |
| ESP32-S3 raw I/Q access | Reported and implemented in public experimental projects; local reproduction pending |
| S3 continuous ring capture | Public implementation exists; throughput and gaps must be measured on target board |
| GFSK/ESB decoding on S3 | Reported for a different device (BenQ ScreenBar Halo), not D8 |
| D8 modulation, bitrate, hopping, CRC, whitening, addresses, association | Unknown |
| D8 transmissions without dongle | Unknown |
| Experimental S3 RF transmission suitable for D8 ACK/pairing | Unknown; not part of initial RX milestone |
| Native USB HID on ESP32-S3 | Officially documented |

Do not treat nRF24L01 compatibility, GFSK, Bluetooth, an ESB packet format, or a particular pairing key combination as confirmed. Establish each independently or mark unsupported. If ordinary Bluetooth or wired USB input works on the actual keyboard, document that simpler recovery route before spending effort on proprietary radio emulation.

## 4. Hardware and development environment

- Primary board: ESP32-S3-N16R8 development board with native USB D+/D− exposed; check actual board schematic, chip revision, flash and PSRAM configuration. GPIO19/20 are native USB D−/D+; do not reassign them in HID builds.
- Device under test: owner's D8 keyboard, charged battery, no original dongle assumed. Photograph label and PCB if needed; disconnect battery before opening case.
- Development host: Debian 13 or Ubuntu Linux; Python 3 virtual environment, NumPy/SciPy, CMake/CTest and ESP-IDF toolchain. Windows interoperability test is secondary.
- Use a known data-capable USB cable. Prefer a separate UART bridge for diagnostics/flashing where available, because native USB Serial/JTAG and USB OTG share the internal PHY on typical S3 boards. Document recovery using bootloader entry and board-specific wiring.
- Optional lab equipment: compatible reference receiver, reference signal source, suitable 2.4 GHz SDR or logic analyzer; optional compatible external RF transceiver only after protocol identification. No purchase assumed.

### Toolchain baseline

Evaluate the ESP-IDF v6.1 development commit `25fe69f946311abdaf9ad56591f25fedbc20ac98` first: the Halo bridge documents this exact dependency. Pin the chosen ESP-IDF revision, submodules, component lockfile, compiler and host Python dependencies; do not assume compatibility with stable 6.1 or another release. Reproduce the upstream radio demonstration and the official HID example separately. Record licenses and private symbols before reuse. A public SDK-only HID build should be possible without the experimental PHY backend.

## 5. Architecture

```text
Owner's D8 keyboard (unmodified)
           |  proprietary RF (format TBD)
           v
+------------------------ ESP32-S3 -------------------------+
| phy_s3 / radio_hal --> iq_capture --> modem / framing      |
|        ^                                  |               |
|        | optional verified link responses v               |
|        +-------------------------- d8_link / association  |
|                                           |               |
|                           d8_protocol --> input_state      |
|                                           |               |
|                              bounded queue + HID scheduler |
|                                           v               |
|                               TinyUSB USB OTG HID         |
+-----------------------------------------------------------+
           | standard USB HID
           v
       Debian / Ubuntu / Windows
```

### Component boundaries

| Component | Responsibility |
|---|---|
| `radio_hal` | Capability reporting, RX lifecycle, tuning, errors, optional gated protocol-specific response capability |
| `phy_s3` | Private PHY initialization, register/ROM dependencies, version and silicon checks, cleanup |
| `iq_capture` | Acquisition, monotonic timestamps, bounded buffers, sample metadata, loss counters |
| `modem` | Evidence-driven modulation detection, demodulation and timing recovery; pure portable DSP where possible |
| `packet_decoder` | Framing, bounds validation, integrity checks and versioned parsed frames |
| `d8_link` | Selected peer association, sequence/retry handling, channel state and reconnect policy |
| `d8_protocol` | Validated D8 payload-to-input interpretation and revision-specific profiles |
| `input_state` | Key/button state, duplicate suppression, relative motion accumulation and fail-safe releases |
| `usb_hid` | TinyUSB descriptors, endpoint readiness, keyboard/mouse/consumer reports, suspend/resume |
| `diagnostics` | State, counters, configuration, capture controls; no normal-mode raw keystroke logging |
| `storage` | Versioned NVS association/configuration; safe reset and corruption handling |

### Execution modes

1. **Diagnostic RX:** no HID output; bounded RF captures of owner's test device, with explicit start/stop and export. Transmission disabled.
2. **Offline replay:** host or test build, deterministic synthetic/authorized fixtures; no RF transmission and no live HID output by default.
3. **Receiver:** native USB HID; accept only validated events from the explicitly enrolled device. No continuous raw streaming or network services.
4. **Lab link-response experiment:** optional separate build; disabled by default; requires evidence of a necessary response and appropriate controlled RF test environment. Not a general RF transmit interface.

At boot, invalid configuration or unsupported chip/SDK must leave RF experimentation and HID output inactive. Diagnostics must not silently enter receiver mode.

## 6. RF feasibility and protocol research

### 6.1 Identify and characterize

Match unit markings and PCB revision to available regulatory exhibits. Determine whether BLE/Classic or a wired HID mode exists. Reproduce the S3 receive baseline independently using a known lab source. Record clock, sample format/rate, gain, effective bandwidth, tuning accuracy, capture duration, gaps and noise floor.

Survey the documented candidate band 2408–2474 MHz and nearby 2.4 GHz channels if justified by evidence and local rules. Compare keyboard-off, idle, wake, known single-key press/release and controlled touchpad trials. Preserve timestamps and acquisition metadata; isolate the test device and exclude unrelated traffic. Establish repeatability across power cycles. A peak alone is not proof of a D8 packet.

### 6.2 Capture and analysis

Use bounded high-rate snapshots and/or an on-device detection path for short bursts. Do not rely on low-rate decimated USB streaming to preserve an unknown high-rate waveform. The documented ESP-SDR USB IQ stream is decimated; characterize sample-rate and aliasing constraints. Prefer internal SRAM for peripheral-accessed capture memory where required; PSRAM is for staged copies or host-transfer buffers only after validating accessibility and timing.

Host analysis should estimate channel occupancy, modulation, symbol rate, frame structure and integrity from independent labeled trials. Establish which fields are invariant, which vary by action, and whether release events and relative motion can be identified. Confirm on held-out captures, not only the training set. Keep protocol conclusions with confidence levels and trace IDs in `docs/RF_PROTOCOL.md`.

### 6.3 Association and link behavior

Determine whether the keyboard emits usable input when no receiver is present. If association is necessary, implement only the documented or experimentally verified normal enrollment for the owned unit. Never assume an ACK format or replay an unidentified packet. If authentication is present, use legitimate enrollment and supported credentials; do not bypass it. Unknown or invalid association must block input delivery. Persist only the information required for normal operation.

If reception-only operation is insufficient, review the hardware, technical and regulatory feasibility of the minimum required link response. Evaluate timing, RF quality, coexistence, power and compliance before enabling over-the-air transmission. Use shielded/conducted tests or other compliant controlled conditions. If the internal radio cannot meet those requirements, report the blocker and evaluate a compatible external transceiver with documented support. Do not present a partial RX demonstration as a functioning receiver.

## 7. USB and input behavior

- Composite HID reports: keyboard (boot-compatible keyboard if practical), relative mouse, consumer control. Distinct report IDs or interfaces; record descriptor design and host behavior.
- Map verified key codes to standard HID usages; handle modifiers and key releases; do not infer unsupported codes. Support supported combinations and document rollover limits.
- Mouse report includes relative X/Y and verified buttons/wheel; preserve motion under normal queue load; split oversized deltas rather than silently truncate.
- Consumer control: supported media/volume usages with explicit release. Local backlight and DPI actions remain local if no RF event exists.
- Handle endpoint busy, USB unplug, host suspend/resume, bootloader entry and reset. Do not accumulate stale actions while the host is unavailable.
- Maintain current input state and reserve a release path even when queues are full. On verified link loss, release all keyboard and mouse buttons, clear relative motion and require fresh validated state before resuming. Tune timeout against observed idle/held-key behavior; do not invent a heartbeat.
- Association is user-initiated by a physical control or local command; never automatically bind to an unknown nearby device. A normal receiver shall not emit HID reports based solely on RF energy or a guessed payload.
- No impersonation of the original vendor USB identity; choose a legitimate development/product USB identity and document distribution constraints.

## 8. Data, interfaces and diagnostics

Proposed internal interfaces (final signatures determined during implementation):

```text
RadioCapabilities { rx, response_supported, frequency_range, sample_formats }
CaptureBlock { version, sequence, monotonic_us, center_hz, sample_rate_hz,
               sample_count, format, gain, discontinuity, samples }
ValidatedFrame { profile_id, peer_id, monotonic_us, sequence, payload }
InputEvent { type, monotonic_us, key_usage, pressed, dx, dy, wheel, buttons }
```

Frame sizes and all indices must be bounded; record discontinuities and reject incomplete frames. Input events are emitted only after association and integrity checks appropriate to the verified protocol. Use versioned capture files (e.g. SigMF-style metadata + binary I/Q) and SHA-256 manifests; include synthetic action labels, source, provenance, sample scaling and capture gaps. Store only controlled test data, not actual passwords or everyday keystrokes. Do not commit raw captures of unrelated traffic or personal input. Provide local diagnostics for RX counts, malformed/dropped frames, queue high-water marks, reset causes, USB state, and current mode. Never log raw keyboard input in production mode.

## 9. Performance targets (provisional, to be measured)

| Metric | Initial acceptance target |
|---|---|
| USB enumeration | Within 3 seconds on supported Linux host after receiver boot, excluding enrollment |
| Input latency | p95 ≤ 30 ms, p99 ≤ 60 ms under nominal conditions; measured from physical input to host event |
| Nominal input fidelity | ≥ 99.9% of labeled expected transitions in ≥ 1,000 mixed input transitions; zero unintended actions or stuck keys |
| Link failure | All held state released within 500 ms of declared link loss; detection timeout established separately from protocol evidence |
| Stability | ≥ 1 hour mixed input/idle operation without crashes, unexpected resets or stuck inputs |
| Reconnect | ≥ 20 power-cycle and USB reconnect trials; restore known association without unexpected input |
| Memory | No allocation failures, no unbounded queues; report internal SRAM/PSRAM usage and stack high-water marks |

Use matched test equipment and a documented timing method. Targets are design goals rather than measurements. If a hardware limitation prevents a target, document measured results and obtain approval for any relaxation. For held keys, verify release on actual link failure without erroneously releasing legitimate long holds.

## 10. Build, repository and testing

```text
esp32-d8-receiver/
  CMakeLists.txt
  sdkconfig.defaults
  sdkconfig.defaults.esp32s3
  dependencies.lock
  main/
  components/{radio_hal,phy_s3,iq_capture,modem,packet_decoder,d8_link,
              d8_protocol,input_state,usb_hid,diagnostics,storage}/
  tools/{capture,analyze_iq,replay,verify_hid}/
  tests/{unit,fixtures,integration,hardware}/
  docs/{SPEC.md,TODO.md,RF_PROTOCOL.md,HARDWARE.md,TEST_PLAN.md,THIRD_PARTY.md}
  .github/workflows/ci.yml
```

Host tests must be runnable without ESP32 hardware: CTest for portable C/C++, pytest for analysis tools, sanitizers, parser fuzzing, deterministic synthetic/replay fixtures, fake radio and USB endpoints. Test RF capture discontinuities, malformed input, CRC/integrity failures where applicable, duplicate events, queue overflow, key releases, USB backpressure, reconnect and NVS failure. Build both safe RX-only and HID/receiver configurations for the pinned SDK; optional experimental response build remains separate. Archive build logs, firmware artifacts, checksums and test results. No hardware result may be inferred from a passing compile or mock test.

On real hardware, validate `lsusb`, HID descriptors, kernel messages and `evtest`/`libinput debug-events` on an isolated test host. Verify all supported keys and mouse actions against a matrix, including press/release, long holds, multi-key combinations, motion direction, scroll, link loss and reset. Do not type credentials during testing. Optional Windows tests use built-in HID drivers. Store dated evidence including commit, board, keyboard revision and test conditions.

## 11. Feasibility gates and release criteria

| Gate | Required evidence | Decision if not met |
|---|---|---|
| G0: baseline | S3 toolchain builds, RX demo works, HID demo enumerates on target | Fix baseline before integration; other host tasks may continue |
| G1: D8 RF | Repeatable activity attributable to the owned D8 with no dongle, or a documented normal enrollment route | Investigate revision/normal pairing; seek owner assistance if needed |
| G2: protocol | Held-out labeled captures decode with demonstrated integrity and input mapping | Continue research; do not implement guessed production decoder |
| G3: link | Legitimate association and usable ongoing traffic, with verified responses if necessary | Report internal-radio blocker; evaluate alternate hardware only as contingency |
| G4: integration | Native USB HID correctly reflects validated live D8 events | Fix pipeline/USB timing; no release claim |
| G5: release | Measured performance, recovery, safety, reproducible build, documented supported revision and limits | Document remaining failures; no production-ready label |

A release requires G0–G5, unless owner explicitly accepts a reduced scope. RX diagnostics, simulated HID and a successful build are intermediate deliverables, not completed dongle emulation.

## 12. Risks and mitigations

| Risk | Mitigation |
|---|---|
| Private PHY calls break with SDK updates | Pin exact version; isolate adapter; check target and symbols; fail explicitly |
| Captures miss short bursts or hopping channels | Measure duty cycle and gaps; use repeatable trials and wider/faster snapshots where feasible |
| D8 requires bidirectional link with stringent timing | Determine normal link behavior; controlled feasibility test; external radio fallback |
| USB PHY contention | Separate UART diagnostics; test capture and HID configurations independently |
| PSRAM or DMA restrictions | Measure actual buffer access; keep critical buffers internal; bounded staging |
| False input/stuck keys | Explicit association, integrity, fail-safe release, negative fixtures |
| RF emissions or inappropriate data collection | RX-only default, controlled authorized experiments, no production raw input logs |
| Third-party license incompatibility | Audit source files, licenses and notices before copying/linking; document distribution terms |

## 13. References (public research, checked 2026-11-25)

- [D8 FCC listing, 2AQRX-D8](https://fccid.io/2AQRX-D8) — candidate model, not proof of owner's hardware revision.
- [D8 user manual exhibit](https://fccid.io/2AQRX-D8/User-Manual/User-Manual-4030483).
- [ESPARGOS ESP-SDR](https://github.com/ESPARGOS/esp-sdr) — experimental radio access; reproduce and review implementation before adoption.
- [ESP-SDR continuous IQ stream documentation](https://github.com/ESPARGOS/esp-sdr/blob/main/docs/iq-stream.md) — decimation and transport limits.
- [ScreenBar Halo bridge](https://github.com/ESPBoards/screenbar-halo-bridge) — experimental S3 radio/modem example and pinned IDF baseline; not a D8 implementation.
- [ESPBoards SDR research write-up](https://www.espboards.dev/blog/esp32-s3-sdr-screenbar-halo/) — reported GFSK reception on another peripheral.
- [ESP-IDF TinyUSB HID example](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_hid).
- [ESP32-S3 USB device stack](https://docs.espressif.com/projects/esp-usb/en/latest/esp32s3/usb_device.html).
