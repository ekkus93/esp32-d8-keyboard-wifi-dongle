/* SPDX-License-Identifier: GPL-3.0-only */
/* Host test: esb_build reproduces the D8 keyboard's captured frames (RF_PROTOCOL.md D2, D3, D8). */
#include <stdio.h>
#include <string.h>
#include "esb.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static unsigned bits_to_u(const uint8_t *b, unsigned w) { unsigned v = 0; for (unsigned k = 0; k < w; k++) v = v << 1 | b[k]; return v; }

static void frame(uint8_t flow, uint16_t want_crc)
{
    const uint8_t pay[11] = {0x51, flow, 0x01, 0, 0, 0, 0, 0x0c, 0, 0, 0x10};
    uint8_t bits[400];
    size_t n = esb_build(bits, sizeof bits, 16, &ESB_D8_PAIRING_ADDR, 0, 1, pay, 11);
    CHECK(n == 16 + 40 + 9 + 88 + 16);
    /* preamble alternates and its last bit differs from the address's first bit (0) */
    for (unsigned k = 1; k < 16; k++) CHECK(bits[k] != bits[k - 1]);
    CHECK(bits[15] == 1 && bits[16] == 0);
    /* header: len 11, pid 0, no_ack 1 = 001011 00 1 */
    CHECK(bits_to_u(bits + 56, 9) == 0x059);
    CHECK(bits_to_u(bits + n - 16, 16) == want_crc);
    /* on-air access code bits from the capture */
    const char *addr = "0101010101011001010101010101010110001110";
    for (unsigned k = 0; k < 40; k++) CHECK(bits[16 + k] == (uint8_t)(addr[k] - '0'));
}

int main(void)
{
    frame(0x40, 0x30D7);   /* search request */
    frame(0x50, 0xA441);   /* power-on request */
    uint8_t big[64] = {0}, bits[100];
    CHECK(esb_build(bits, sizeof bits, 8, &ESB_D8_PAIRING_ADDR, 0, 0, big, 11) == 0);   /* too long */
    CHECK(esb_build(bits, sizeof bits, 8, &ESB_D8_PAIRING_ADDR, 0, 0, big, 33) == 0);   /* > max payload */
    printf(fails ? "test_esb: %d failures\n" : "test_esb: OK\n", fails);
    return fails != 0;
}
