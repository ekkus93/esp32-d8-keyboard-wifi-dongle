# Hardware

Status: partial identification (TODO P1.1, P1.3, P1.4). Unknowns are marked as such.

## Receiver board (ESP32-S3)

Observed 2026-11-25 via `esptool chip_id` / `flash_id` (esptool v5.7.0) on the development host:

| Item | Value | Source |
|---|---|---|
| Chip | ESP32-S3 (QFN56), revision v0.2 | esptool |
| Features | Wi-Fi, BT 5 (LE), dual core + LP core, 240 MHz | esptool |
| Flash | 16 MB (manufacturer 0x68, device 0x4018) | esptool flash_id |
| PSRAM | 8 MB embedded (AP_3v3) | esptool |
| Crystal | 40 MHz | esptool |
| Host connection | Native USB Serial/JTAG (`303a:1001`), `/dev/ttyACM0` | lsusb |
| UART bridge | None connected | lsusb |
| Board model / schematic | Unknown | — |

Implication: diagnostics and flashing currently share the native USB PHY with any future USB OTG HID build. A separate UART bridge (GPIO43/44) is needed before HID builds (SPEC §4).

## Keyboard (D8)

| Item | Value | Source / confidence |
|---|---|---|
| FCC ID (candidate) | 2AQRX-D8, Shenzhen Dingyuecheng Electronics, Part 15.249, 2408–2474 MHz, granted 2018-10-09 | fccid.io grant page; owner's unit label not yet matched |
| RF SoC | Telink TLSR8368 | Owner-reported PCB marking |
| SPI flash | Puya P25Q40H (4 Mbit / 512 KB SPI NOR) | Owner-reported PCB marking |
| Bluetooth | Not expected; host BT scan with keyboard on found no keyboard-like device | Host `bluetoothctl` scan, 20 s; owner states 2.4 GHz proprietary only |
| PCB revision, photos | Not recorded | — |

### TLSR8368 (vendor documentation, not yet verified on unit)

- 2.4 GHz proprietary RF SoC aimed at wireless keyboards.
- Configurable link rate: 2 Mbps or 250 kbps. Sensitivity ~−88 dBm @2 Mbps, ~−97 dBm @250 kbps; TX up to +6 dBm.
- Modulation: GFSK/FSK per sibling TLSR8366 datasheet; not confirmed for 8368.
- On-air packet format, addressing, hopping and pairing: **unknown**; no public specification found. Telink keyboard/dongle reference designs may use ACK-based bidirectional links — hypothesis only (relevant to SPEC §6.3 / TODO P6.5).

### P25Q40H

Likely holds TLSR8368 firmware and possibly pairing state. Not read. Reading it would require battery-disconnected in-circuit or off-board SPI access; owner approval required.

## References

- [TLSR8368 product brief](http://wiki.telink-semi.cn/doc/ds/PB_TLSR8368-E_Product%20Brief%20for%20Telink%202.4GHz%20RF%20System-On-Chip%20Solution%20TLSR8368.pdf)
- [TLSR8368 datasheet](https://www.mouser.com/datasheet/2/1039/DS_TLSR8368_E_Datasheet_for_Telink_2_4GHz_RF_Syste-3070491.pdf)
- [TLSR8366 datasheet](https://wiki.telink-semi.cn/doc/ds/DS-TLSR8366-E_Datasheet%20for%20Telink%202.4GHz%20RF%20System-On-Chip%20Solution%20TLSR8366.pdf)
- [Telink HID solution](https://wiki.telink-semi.cn/wiki/solution/HID-Solution/)
- [Community Telink pairing notes](https://gist.github.com/VoxelPrismatic/4ff43979394ec042a8f3009de2acd9cc/e993964a64140428354dbe213e70b2eaebf410e8)
