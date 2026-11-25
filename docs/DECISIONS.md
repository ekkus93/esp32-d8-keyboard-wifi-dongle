# Decision record

| Date | Decision | By | Rationale / conditions |
|---|---|---|---|
| 2026-11-25 | Bluetooth / wired workaround not applicable | Owner | D8 is 2.4 GHz proprietary only; host BT scan found no keyboard. |
| 2026-11-25 | Pursue ESP32-S3-only receiver (option 1) rather than nRF24L01+ contingency | Owner | Protocol identified as nRF-ESB-compatible framing (RF_PROTOCOL.md D1–D3); main risk is S3 real-time reply timing. nRF24L01+ remains the documented fallback. |
| 2026-11-25 | Project license changed Apache-2.0 → GPL-3.0-only | Owner | Allows reuse of ESP-SDR / Halo bridge S3 PHY code. See THIRD_PARTY.md. |
| 2026-11-25 | RF transmission experiments approved | Owner | Lab build only, disabled by default in normal builds; short low-power 2.4 GHz bursts, only to establish a link with the owner's keyboard; only frames whose format is verified (RF_PROTOCOL.md) or documented by the Telink reference. |
| 2026-11-25 | Second ESP32-S3 board provided for TX/RX cross-checks | Owner | Board B may be flashed freely. |
