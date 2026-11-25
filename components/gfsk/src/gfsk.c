/* SPDX-License-Identifier: GPL-3.0-only */
#include "gfsk.h"

#include <math.h>
#include <stdbool.h>

#include "s3_iq.h"

#define SPB      GFSK_SPB
#define COS_BITS 10u
#define COS_SIZE (1u << COS_BITS)
#define PI       3.14159265358979323846   /* M_PI is not standard C */
_Static_assert(GFSK_RATE % GFSK_BIT_RATE == 0, "whole DAC samples per bit");

/* Phase through one bit for each (previous, current, next) pattern. BT 0.5
 * smears an edge by less than a bit, so three bits are enough. Full turn = 2^32.
 */
static uint32_t bit_phase[8][SPB + 1];
static int16_t cos_table[COS_SIZE];
static bool ready;
static double deviation_hz = GFSK_DEVIATION;   /* esp32-d8-receiver: runtime-settable */
static uint32_t if_step;                       /* esp32-d8-receiver: digital IF, phase per DAC sample */
static int64_t chirp_q16;                      /* esp32-d8-receiver: frequency ramp, phase per sample^2, Q16 */

/* IF + chirp phase at absolute DAC sample n (wraps mod 2^32 like every other phase here). */
static inline uint32_t extra_phase(size_t n)
{
    return (uint32_t)(n * if_step) + (uint32_t)(((int64_t)n * (int64_t)n * chirp_q16) >> 16);
}

/* Half a Gaussian-filtered edge, 0.5 * erf, from -2 to +2 bits. A bit's shape is two of these, so tabling
 * it means init needs 1280 erf calls instead of six per sample of every pattern. Float gives the same DAC words
 * as double.
 */
#define EDGE_HALF (2 * (int)SPB)
static float edge[2 * EDGE_HALF];

static double shaped(int t, int start)
{
    return (double)edge[t - start + EDGE_HALF] - (double)edge[t - start - (int)SPB + EDGE_HALF];
}

void gfsk_init(void)
{
    const double a = PI * GFSK_BT * sqrt(2.0 / log(2.0)) / SPB;
    for (int x = -EDGE_HALF; x < EDGE_HALF; x++) {
        edge[x + EDGE_HALF] = (float)(0.5 * erf(a * x));
    }
    const double turn = 4294967296.0;
    for (unsigned p = 0; p < 8; p++) {
        double cycles = 0;
        bit_phase[p][0] = 0;
        for (unsigned s = 0; s < SPB; s++) {
            double level = 0;
            for (int j = -1; j <= 1; j++) {
                int bit = (p >> (1 - j)) & 1;
                level += (bit ? 1.0 : -1.0) * shaped((int)s, j * (int)SPB);
            }
            cycles += level * deviation_hz / GFSK_RATE;
            bit_phase[p][s + 1] = (uint32_t)(int64_t)llround(cycles * turn);
        }
    }
    for (unsigned k = 0; k < COS_SIZE; k++) {
        cos_table[k] = (int16_t)lround(GFSK_AMPLITUDE * cos(2 * PI * k / COS_SIZE));
    }
    ready = true;
}

void gfsk_prepare(gfsk_t *g, const uint8_t *bits, size_t nbits, size_t lead)
{
    if (!ready) {
        gfsk_init();
    }
    if (nbits > GFSK_MAX_BITS) {
        nbits = GFSK_MAX_BITS;
    }
    g->lead = lead;
    g->nbits = nbits;
    uint32_t phase = 0;
    for (size_t k = 0; k < nbits; k++) {
        /* Past the ends, repeat the edge bit, so the first and last bits reach full deviation. */
        unsigned prev = bits[k ? k - 1 : 0] & 1u;
        unsigned next = bits[k + 1 < nbits ? k + 1 : k] & 1u;
        g->pattern[k] = (uint8_t)(prev << 2 | (bits[k] & 1u) << 1 | next);
        g->phase[k] = phase;
        phase += bit_phase[g->pattern[k]][SPB];
    }
}

size_t gfsk_length(const gfsk_t *g)
{
    return g->lead + g->nbits * SPB;
}

static inline unsigned cos_index(uint32_t phase)
{
    return ((phase + (1u << (31 - COS_BITS))) >> (32 - COS_BITS)) & (COS_SIZE - 1);
}

void gfsk_sample(const gfsk_t *g, size_t n, int16_t *i, int16_t *q)
{
    if (n < g->lead || n >= gfsk_length(g)) {
        *i = *q = 0;
        return;
    }
    size_t k = (n - g->lead) / SPB, s = (n - g->lead) % SPB;
    unsigned idx = cos_index(g->phase[k] + bit_phase[g->pattern[k]][s + 1] + extra_phase(n));
    *i = cos_table[idx];
    *q = cos_table[(idx - COS_SIZE / 4) & (COS_SIZE - 1)];   /* sin = cos 90 degrees back */
}

void gfsk_fill(const gfsk_t *g, size_t first, uint32_t *out, size_t count)
{
    size_t n = first, end = gfsk_length(g);
    for (; count && n < g->lead; count--, n++) {
        *out++ = 0;
    }
    if (count && n < end) {
        size_t k = (n - g->lead) / SPB, s = (n - g->lead) % SPB;
        for (; count && k < g->nbits; k++, s = 0) {
            const uint32_t *row = bit_phase[g->pattern[k]];
            uint32_t base = g->phase[k];
            for (; count && s < SPB; s++, count--, n++) {
                unsigned idx = cos_index(base + row[s + 1] + extra_phase(n));
                *out++ = s3_iq_pack(cos_table[idx], cos_table[(idx - COS_SIZE / 4) & (COS_SIZE - 1)]);
            }
        }
    }
    for (; count; count--) {
        *out++ = 0;
    }
}

void gfsk_set_deviation(double hz)
{
    deviation_hz = hz;
    gfsk_init();   /* rebuild the phase tables */
}

void gfsk_set_if(double hz)
{
    if_step = (uint32_t)(int64_t)llround(hz / GFSK_RATE * 4294967296.0);
}

void gfsk_set_chirp(double hz_per_us)
{
    /* f(t) = rate * t  ->  phase(n) = 0.5 * rate * (n / fs)^2 turns. */
    double rate = hz_per_us * 1e6;   /* Hz per second */
    double turns_per_n2 = 0.5 * rate / ((double)GFSK_RATE * (double)GFSK_RATE);
    chirp_q16 = llround(turns_per_n2 * 4294967296.0 * 65536.0);
}
