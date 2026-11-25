/* SPDX-License-Identifier: GPL-3.0-only */
/* GFSK modulator at the S3's 40 MS/s DAC rate.
 *
 * From screenbar-halo-bridge 67deba5 lib/modem (halo_gfsk), modified 2026-11-25 for
 * esp32-d8-receiver: 2 Mbit/s, +-500 kHz (D8 / Telink 2M mode, docs/RF_PROTOCOL.md O5),
 * renamed, and no longer tied to the Halo ESB frame size.
 *
 * Every sample can be computed on its own because the S3's two DAC banks
 * overlap and get filled out of order.
 */
#pragma once
#include <stddef.h>
#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define GFSK_RATE      40000000u
#define GFSK_BIT_RATE  2000000u
#define GFSK_SPB       (GFSK_RATE / GFSK_BIT_RATE)
#define GFSK_DEVIATION 500000.0
#define GFSK_BT        0.5
#define GFSK_MAX_BITS  640       /* preamble + access code + header + 32-byte payload + CRC, with room */
#define GFSK_AMPLITUDE 128   /* phy_s3's TX gain table assumes samples at this level */

typedef struct {
    size_t lead;   /* silent samples before the preamble */
    size_t nbits;
    uint8_t pattern[GFSK_MAX_BITS];
    uint32_t phase[GFSK_MAX_BITS];   /* carrier phase where each bit starts */
} gfsk_t;

/* Builds the tables. Call at boot so the first command isn't slower; prepare()
 * calls it anyway if you forget.
 */
void gfsk_init(void);

void gfsk_prepare(gfsk_t *g, const uint8_t *bits, size_t nbits, size_t lead);

size_t gfsk_length(const gfsk_t *g);

/* Zero outside the frame so the DAC is silent there. */
void gfsk_sample(const gfsk_t *g, size_t n, int16_t *i, int16_t *q);

/* Samples first .. first + count as packed DAC words (s3_iq_pack). Same values
 * as gfsk_sample, without a divide per sample: loading the DAC is 29k samples.
 */
void gfsk_fill(const gfsk_t *g, size_t first, uint32_t *out, size_t count);

#ifdef __cplusplus
}
#endif
