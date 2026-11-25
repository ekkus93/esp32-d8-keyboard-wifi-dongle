# Third-party code and references

Project license: **GPL-3.0-only** (see `LICENSE`). Changed from Apache-2.0 on 2026-11-25 by owner decision so that GPL-licensed ESP32-S3 radio code can be reused.

## Reused or reusable code

| Source | Revision | License | Use |
|---|---|---|---|
| [ESPARGOS/esp-sdr](https://github.com/ESPARGOS/esp-sdr) | `3ab3b65` | GPL-3.0-or-later (esp-dsp-derived `.S` files Apache-2.0) | Used unmodified as external research firmware (RX captures). Code may be reused with headers kept. |
| [ESPBoards/screenbar-halo-bridge](https://github.com/ESPBoards/screenbar-halo-bridge) | `67deba5` | GPL-3.0-only (diagrams in `docs/img` CC BY-NC 4.0 — do not copy) | Reference for S3 PHY TX/RX ring (`s3/components/phy_s3`). Code may be reused with headers kept. |
| [dckiller51/esphome-yeelight-led-screen-light-bar](https://github.com/dckiller51/esphome-yeelight-led-screen-light-bar) "Remote TLSR8368 SDK" | `main` (2026-11-25) | Apache-2.0 (compatible with GPL-3.0) | Telink RF driver headers, reference only so far. |
| [lamperez/xiaomi-lightbar-nrf24](https://github.com/lamperez/xiaomi-lightbar-nrf24) | `main` (2026-11-25) | GPL-3.0 | TLSR8368 framing/CRC reference only. |

Every reused file keeps its original copyright and SPDX header and is listed here with the upstream path and revision.

## Reference-only (no code may be copied)

| Source | Status | Use |
|---|---|---|
| Telink KM SDK snapshot in [stephanosio/js_km_proj](https://github.com/stephanosio/js_km_proj) @ `4d9960b` (local copy `~/esp/ref/telink_km`) | **No license** — all rights reserved | Protocol facts only (frame layouts, pairing procedure, channel tables). Our implementation is written independently. |
| Telink TLSR8368 datasheet (DS-TLSR8368-E20, local `docs/ref/`, git-ignored) | Vendor document, not redistributed | Hardware reference. |

## Build dependencies

| Component | Revision | License |
|---|---|---|
| ESP-IDF | `25fe69f946311abdaf9ad56591f25fedbc20ac98` | Apache-2.0 |
| TinyUSB (via ESP-IDF / esp_tinyusb) | as pinned by ESP-IDF | MIT |
