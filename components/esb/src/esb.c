/* SPDX-License-Identifier: GPL-3.0-only */
#include "esb.h"

#include <stdbool.h>

const esb_addr_t ESB_D8_PAIRING_ADDR = {.onair = {0x55, 0x59, 0x55, 0x55, 0x8E}};

uint16_t esb_crc16_bits(const uint8_t *bits, size_t nbits, uint16_t init)
{
    uint16_t c = init;
    for (size_t k = 0; k < nbits; k++) {
        bool x = ((c >> 15) & 1u) ^ (bits[k] & 1u);
        c = (uint16_t)(c << 1);
        if (x) {
            c ^= 0x1021u;
        }
    }
    return c;
}

static size_t put_bits(uint8_t *bits, size_t at, uint32_t value, unsigned width)
{
    for (unsigned k = 0; k < width; k++) {
        bits[at + k] = (uint8_t)((value >> (width - 1 - k)) & 1u);
    }
    return at + width;
}

size_t esb_build(uint8_t *bits, size_t max_bits, size_t preamble_bits, const esb_addr_t *addr,
                 uint8_t pid, uint8_t no_ack, const uint8_t *payload, uint8_t len)
{
    if (len > ESB_MAX_PAYLOAD) {
        return 0;
    }
    size_t body = ESB_ADDR_LEN * 8u + 9u + 8u * len;
    size_t total = preamble_bits + body + 16u;
    if (total > max_bits) {
        return 0;
    }
    /* Preamble alternates and ends on the complement of the access code's first bit. */
    uint8_t first = (uint8_t)(addr->onair[0] >> 7);
    for (size_t k = 0; k < preamble_bits; k++) {
        size_t from_end = preamble_bits - 1 - k;
        bits[k] = (uint8_t)((first ^ 1u) ^ (from_end & 1u));
    }
    size_t n = preamble_bits;
    for (unsigned b = 0; b < ESB_ADDR_LEN; b++) {
        n = put_bits(bits, n, addr->onair[b], 8);
    }
    n = put_bits(bits, n, (uint32_t)(len & 0x3fu) << 3 | (uint32_t)(pid & 3u) << 1 | (no_ack & 1u), 9);
    for (unsigned b = 0; b < len; b++) {
        n = put_bits(bits, n, payload[b], 8);
    }
    uint16_t crc = esb_crc16_bits(bits + preamble_bits, body, 0xFFFFu);
    n = put_bits(bits, n, crc, 16);
    return n;
}
