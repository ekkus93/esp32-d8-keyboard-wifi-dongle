/* SPDX-License-Identifier: GPL-3.0-only */
/* Enhanced-ShockBurst-style frame builder for the D8 / Telink 2 Mbps link.
 *
 * On-air layout (docs/RF_PROTOCOL.md D1-D3): alternating preamble, 5-byte access
 * code, 9-bit header (6-bit length, 2-bit PID, NO_ACK), payload (MSB-first bytes),
 * CRC-16 (poly 0x1021, init 0xFFFF, MSB-first over access code + header + payload).
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ESB_ADDR_LEN    5u
#define ESB_MAX_PAYLOAD 32u

/* Access code as it appears on air, MSB-first bytes (D8 pairing pipe: 55 59 55 55 8E). */
typedef struct {
    uint8_t onair[ESB_ADDR_LEN];
} esb_addr_t;

extern const esb_addr_t ESB_D8_PAIRING_ADDR;

/* CRC-16/0x1021 over a bit array (one bit per byte, values 0/1), MSB-first. */
uint16_t esb_crc16_bits(const uint8_t *bits, size_t nbits, uint16_t init);

/* Writes one bit per byte into bits[]; returns the number of bits, or 0 if it
 * would not fit in max_bits or len > ESB_MAX_PAYLOAD.
 * preamble_bits: alternating bits before the access code; the last one is the
 * complement of the access code's first bit.
 */
size_t esb_build(uint8_t *bits, size_t max_bits, size_t preamble_bits, const esb_addr_t *addr,
                 uint8_t pid, uint8_t no_ack, const uint8_t *payload, uint8_t len);

#ifdef __cplusplus
}
#endif
