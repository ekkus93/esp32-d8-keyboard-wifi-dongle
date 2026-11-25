# D8 Wireless Keyboard — TLSR8368 Research Handoff

**Date:** 2026-11-25  
**Project:** `esp32-d8-receiver`  
**Audience:** Claude Code / firmware developer  
**Related documents:** [SPEC.md](SPEC.md), [TODO.md](TODO.md)  
**Status:** New chipset evidence; D8 protocol and ESP32-S3 interoperability remain unverified.

## 1. Executive summary

The owner reports that the D8 mini wireless keyboard contains a Telink Semiconductor **TLSR8368** SoC. The original USB receiver is missing. This narrows the physical-layer investigation substantially: the radio is no longer an entirely unidentified chipset, and Telink publishes a datasheet describing its RF capabilities and baseband framing.

The primary objective remains an ESP32-S3-N16R8-only replacement USB receiver using experimental undocumented RF/PHY access for reception and native USB HID for keyboard, touchpad and consumer-control reports. An external nRF24L01+ is a contingency, not a new mandatory hardware dependency.

An existing reverse-engineering project for a Xiaomi light bar reportedly uses the same TLSR8368 and demonstrates interoperability with an nRF24L01+. This is a strong research lead, not evidence that the D8 uses that device's protocol or that the ESP32-S3 can already receive it.

The following findings are carried forward from the preceding research discussion. Verify the original sources, device marking, exact silicon variant and repository revisions before treating the details as implementation requirements. No new RF captures, firmware inspection or hardware tests were performed for this handoff.

## 2. TLSR8368 chipset

The TLSR8368 is a Telink 2.4 GHz proprietary wireless SoC suited to low-power peripherals such as keyboards and remote controls. Do not assume standard Bluetooth interoperability merely because other Telink chip families support Bluetooth.

### Reported specifications

| Parameter | Value |
|---|---|
| Manufacturer | Telink Semiconductor |
| Processor | 32-bit RISC, up to 48 MHz |
| Program memory | 16 KB OTP (one-time programmable) |
| SRAM | 6 KB |
| EEPROM | 256 bytes on E02 variant only |
| Frequency range | 2400–2483.5 MHz |
| Modulation family | FSK / GFSK |
| Supported data rates | 250 kbps and 2 Mbps |
| Transmit power | Typically +6 dBm |
| Receiver sensitivity at 250 kbps | Approximately −97 dBm |
| Receiver sensitivity at 2 Mbps | Approximately −88 dBm |
| Debug interface | Single Wire Slave (SWS) |
| Other interfaces | SPI / I²C, package dependent |
| Deep sleep current | Approximately 0.7 µA |

**Primary reference:** [TLSR8368 datasheet](https://wiki.telink-semi.cn/doc/ds/DS_TLSR8368-E_Datasheet%20for%20Telink%202.4GHz%20RF%20System-On-Chip%20Solution%20TLSR8368.pdf).

These are reported chip capabilities, not measured D8 settings. The CPU frequency, radio rate, transmit power, EEPROM availability and other parameters must not be assumed for the specific keyboard. Check datasheet revision and test conditions when citing numerical values.

## 3. Documented physical-layer framing

The preceding research identified section 4.3.1 (printed page 38) of the TLSR8368 datasheet as describing its 2 Mbps baseband frame:

```text
+----------+----------------+------------------+----------+
| Preamble | Access address | PDU / payload    | CRC      |
| 1 byte   | 3–5 bytes      | 0–63 bytes       | 1–2 bytes|
+----------+----------------+------------------+----------+
```

The reported transmission duration is 20–284 microseconds. Confirm the diagram, permitted field combinations, timing and bit ordering against the original datasheet. The chip also reportedly provides access-address recognition, CRC validation and channel/frequency-hopping capabilities.

This is a chip-level frame description, **not** a decoded D8 application packet format. It does not establish the keyboard's selected rate, channel plan, whitening, CRC configuration, payload layout, association mechanism, acknowledgments or any security features. In particular, the frame should not be equated automatically with Nordic Enhanced ShockBurst (ESB).

The existing FCC filing for a D8 model (FCC ID `2AQRX-D8`) specifies 2408–2474 MHz. Match the owner's hardware revision to that filing before treating that range as exhaustive.

- [FCC D8 equipment authorization exhibits](https://fccid.io/2AQRX-D8)
- [FCC D8 internal photographs](https://fccid.io/2AQRX-D8/Internal-Photos/Internal-Photos-4030486)

## 4. Related reverse-engineering projects

### 4.1 Xiaomi light bar — same reported chipset

- [lamperez/xiaomi-lightbar-nrf24](https://github.com/lamperez/xiaomi-lightbar-nrf24): reverse engineering of a Xiaomi light bar reportedly using a TLSR8368, including radio modulation, framing, frequency hopping and checksums. Demonstrates communication with an nRF24L01+ for that product.
- [BAJ810/xiaomi-lightbar-nrf24-esp32](https://github.com/BAJ810/xiaomi-lightbar-nrf24-esp32): related ESP32 implementation with an external nRF24L01 radio.

Review these repositories for reusable radio-level concepts, assumptions, evidence and licensing. Compare their documented chipset and RF settings with Telink's datasheet. Do not import Xiaomi-specific addressing, checksums, commands or hopping behavior as D8 facts. Neither project is known to implement the D8 keyboard protocol.

### 4.2 ESP32-S3 experimental radio and USB resources

- [ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr): experimental raw radio I/Q access and SDR reception.
- [cyber-murmel/esp-sdr-zephyr](https://github.com/cyber-murmel/esp-sdr-zephyr): related experimental capture/playback research.
- [ESPBoards/screenbar-halo-bridge](https://github.com/ESPBoards/screenbar-halo-bridge): another proprietary wireless protocol implementation using ESP32-S3, with GFSK modem and radio-driver research.
- [Junyi-99/halo-esp32s3](https://github.com/Junyi-99/halo-esp32s3): related experimental radio work.
- [Espressif TinyUSB HID example](https://github.com/espressif/esp-idf/tree/master/examples/peripherals/usb/device/tusb_hid): official native USB HID baseline.

The experimental radio mechanisms are private, reverse-engineered hardware/PHY interfaces, not a stable, officially supported ESP-IDF SDR API. Retain the existing isolated radio abstraction and SDK pinning requirements. Reproduce upstream behavior on the actual target before claiming D8 compatibility. The previously proposed ESP-IDF commit `25fe69f946311abdaf9ad56591f25fedbc20ac98` is a candidate to verify, not a validated local build.

## 5. Telink development resources

| Resource | Why it matters |
|---|---|
| [Telink 2.4G SoC GitHub](https://github.com/Telink-demo/2.4G-SoC) | Manufacturer documentation and development material |
| [Telink TLSR823x resources](https://github.com/Telink-demo/TLSR823x) | Previously identified lead for a related TLSR8369 keyboard reference SDK |
| [Telink proprietary 2.4 GHz SDK handbook](https://doc.telink-semi.cn/doc/en/software/res/sdk/2.4GHz%20Proprietary/telink_2.4ghz_proprietary_sdk_en/telink_2.4ghz_proprietary_sdk_developer_handbook_en/) | Proprietary link-layer terminology and reference architecture |
| [Telink Linux/macOS Burning and Debugging Tool documentation](https://doc.telink-semi.cn/doc/en/software/res/tools/bdt_linux_mac/bdt_linux_mac_en/) | Reported TLSR8368 debugging support and OTP access capabilities |

A related Telink keyboard reference design is a plausible research lead. Its relationship to the TLSR8368 and actual D8 firmware has not been established. Verify supported chips, SDK dates, licensing and applicability before relying on any reference code.

## 6. Optional non-destructive hardware investigation

The TLSR8368 exposes Telink's SWS debug interface. If accessible on the owner's keyboard, manufacturer-supported, read-only inspection may help identify the hardware and firmware configuration. Tool support does not imply that a particular production unit permits memory reads.

Before considering this branch, document the exact chip marking, package, board revision, pad labels, electrical specifications and official tool support. Obtain the owner's approval for any physical connection. Do not write, erase, program OTP, disable protections or bypass access restrictions. Do not distribute proprietary firmware dumps. This is an optional research path and should not block independent ESP32-S3 development.

## 7. Evidence ledger

| Question | Current status |
|---|---|
| Keyboard model | D8, owner identified |
| Original receiver | Missing |
| RF chipset | TLSR8368, owner reports physical identification; board photo and suffix not yet recorded here |
| Radio modulation and supported rates | Datasheet-level leads: FSK/GFSK, 250 kbps / 2 Mbps |
| D8 actual modulation and rate | Unknown |
| Generic chipset framing | Documented; verify source details |
| D8-specific frame configuration and application format | Unknown |
| D8 association and bidirectional requirements | Unknown |
| D8 security/encryption | Unknown |
| D8 operation without original receiver | Not measured |
| Same-chip interoperability with an external radio | Reported for Xiaomi light bar, not D8 |
| ESP32-S3 SDR reception | Reported upstream; not reproduced locally |
| ESP32-S3-only D8 receiver | Not demonstrated |
| Native USB HID | Officially supported; target integration remains to be tested |
| Public D8-specific protocol implementation | None identified in preceding searches; absence is not proof that none exists |

## 8. Requested updates to SPEC.md and TODO.md

Claude Code should reconcile this evidence with the existing documents rather than replace the project architecture or mark a research gate complete prematurely.

### SPEC.md changes

- Update the purpose/evidence sections: replace the statement that the chipset is completely unidentified with the owner's reported TLSR8368 identification; retain the requirement to document the board and suffix.
- Add datasheet, official Telink resources and the same-chip Xiaomi projects to the research references.
- Record documented candidate physical-layer capabilities separately from verified D8 on-air behavior.
- Keep ESP32-S3-only reception and native USB HID as the main target; maintain the external radio contingency.
- Preserve authorized-device-only operation, explicit association, fail-closed behavior, RX-first default and controlled hardware gates.

### TODO.md changes

- [ ] Extend P1.4: document the TLSR8368 physical marking, suffix, board revision and datasheet revision. The owner's report is useful evidence but is not a completed hardware-validation gate.
- [ ] Audit Telink public documentation and related keyboard reference SDKs; record relevant differences and licensing.
- [ ] Review the Xiaomi light-bar projects for chipset-level interoperability evidence and distinguish it from product-specific behavior.
- [ ] Extend P2/P3 feasibility planning: determine whether the experimental ESP32-S3 acquisition path has adequate bandwidth and sampling performance for the documented candidate modes; retain actual hardware validation.
- [ ] Update `docs/RF_PROTOCOL.md` with an evidence table that separates chip capabilities, candidate settings and actual measurements on the owner's device.
- [ ] Keep independent USB HID, host-side tests, reproducible CI and radio abstraction work moving when owner-assisted hardware evidence is unavailable.
- [ ] Maintain an explicit decision record for S3-only feasibility versus an external compatible radio fallback. Do not purchase or require extra hardware without discussing the need with the owner.
- [ ] Optional: evaluate safe, read-only manufacturer-supported diagnostics; do not change the keyboard or attempt to defeat protections.

Retain existing task IDs where possible and append new subtasks rather than renumbering completed work. No checkboxes are completed by this handoff alone.

## 9. Acceptance and development constraints

- Treat all upstream links and technical claims as research leads until source-reviewed and, where applicable, independently reproduced.
- Do not fabricate captures, successful pairing, decoded reports, interoperability or bench results. Keep blocked hardware tasks unchecked and document the missing evidence.
- Limit experiments to the owner's device and controlled bench environment. Normal receiver functionality must reject unknown or unassociated devices and must not inject unidentified radio traffic as USB input.
- Keep experimental transmission disabled by default. Do not bypass pairing/authentication or modify the original keyboard's firmware.
- Maintain the existing project's reproducible builds, host tests, licensing review and hardware acceptance gates.

**Bottom line:** the TLSR8368 identification is a meaningful improvement in the research baseline. It does not establish the D8 protocol or guarantee an ESP32-S3-only receiver. The next useful work is to reconcile primary documentation and related reference implementations with the existing spec, reproduce the receive baseline, and obtain revision-specific evidence from the owner's keyboard.
