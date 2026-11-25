/* SPDX-License-Identifier: GPL-3.0-only */
/* ESP32-S3 radio as a 2405 MHz SDR: play a waveform through the Wi-Fi DAC and
 * capture raw IQ from its ADC.
 *
 * Built on undocumented PHY functions and registers. Only valid with ESP-IDF
 * 25fe69f946; don't upgrade as cleanup. The register sequences are kept exactly
 * as in halo-esp32s3 (121344e), which takes them from ESPARGOS/esp-sdr and
 * cyber-murmel/esp-sdr-zephyr, because that's what is proven on real boards.
 * Adapted and modified for this project.
 *
 * esp32-d8-receiver modifications (2026-11-25), from screenbar-halo-bridge 67deba5:
 * runtime carrier (phy_s3_set_carrier).
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "sdkconfig.h"

#define PHY_S3_TX_SAMPLES      26720u   /* 668 us at 40 MS/s: fits an 81-bit frame plus silence */
#define PHY_S3_CAPTURE_SAMPLES 16380u   /* 1.02 ms at 16 MS/s: one bank */
#define PHY_S3_GAIN_MAX        10u

/* The ring and TX timing count CPU cycles; the ring's ADC runs off the same crystal. */
#define PHY_S3_CYCLES_PER_US           ((unsigned)CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ)
#define PHY_S3_RING_CYCLES_PER_SAMPLE  15u   /* 16 MS/s */

typedef struct {
    bool captured;            /* false if the ADC never finished */
    uint32_t turnaround_us;   /* end of TX to start of capture */
} phy_s3_tx_result_t;

/* Writes packed IQ words (halo_iq_pack) for DAC samples first .. first + count.
 * Ranges, not single samples, so the source can run without a call per sample.
 */
typedef void (*phy_s3_fill_fn)(size_t first, uint32_t *out, size_t count, void *ctx);

void phy_s3_init(void);

/* Retune to mhz + khz (kHz offset), recalibrate and settle the receiver (~3 ms+).
 * The next send or listen uses the new carrier.
 */
void phy_s3_set_carrier(unsigned mhz, int khz);

/* Fill the DAC banks. Separate from send so it can be timed on its own. */
void phy_s3_load(phy_s3_fill_fn fill, void *ctx);

/* Load, transmit, then capture straight away: the lamp answers ~184 us after we
 * stop. One attempt; retries are the caller's job. The capture overwrites the
 * first DAC bank, which is why this always reloads.
 */
phy_s3_tx_result_t phy_s3_send(phy_s3_fill_fn fill, void *ctx, unsigned gain);

/* Re-tune and settle the receiver (3 ms). halo-esp32s3 does this before and
 * after every command, never between attempts.
 */
void phy_s3_reset_rx(void);

/* Capture without transmitting. */
bool phy_s3_listen(void);

/* The latest capture, PHY_S3_CAPTURE_SAMPLES words. Valid until the next send,
 * listen or ring start: the ring writes the same bank.
 */
const uint32_t *phy_s3_capture(void);

/* Spare bank: neither TX nor capture uses it. */
#define PHY_S3_SPARE_BYTES 0x10000u
void *phy_s3_spare(void);

/* Continuous capture with no gaps, using the ADC's ring mode (bit 14 of its
 * control register, found by probing; not in esp-sdr). The ADC writes a
 * 16,384-sample ring nonstop; ring_next hands it to the other bank shortly
 * before each wrap, timed to the cycle so every flip lands on the same offset.
 * Each bank then holds one whole lap while the CPU reads it.
 *
 * Call start, next and stop from one core (they time with its cycle counter),
 * and transmit nothing while the ring runs.
 */
#define PHY_S3_RING_SAMPLES 16384u

typedef struct {
    const uint32_t *samples;   /* a whole bank, PHY_S3_RING_SAMPLES words */
    size_t from, to;           /* samples[from..to), or samples[from..RING) then samples[0..to) when from >= to */
} phy_s3_chunk_t;

typedef struct {
    uint32_t next;     /* cycle count of the next flip */
    size_t flip;       /* ring offset every flip lands on */
    unsigned bank;     /* bank the ADC writes now */
} phy_s3_ring_t;

/* Starts the ring and returns the first chunk, which is shorter than a lap. */
bool phy_s3_ring_start(phy_s3_ring_t *ring, phy_s3_chunk_t *first);

/* Waits for the next flip and returns the lap that just finished. It must be
 * read before the following call, which gives that bank back to the ADC.
 * False if we arrived too late to flip on time: the ring is stopped and the
 * samples since the last chunk are lost.
 */
bool phy_s3_ring_next(phy_s3_ring_t *ring, phy_s3_chunk_t *chunk);

void phy_s3_ring_stop(void);

/* Re-force the RX gain. esp-sdr-zephyr saw Wi-Fi's AGC take it over during long listening. */
void phy_s3_ring_regain(void);

/* The PHY recalibrated since the ring started: restart it to redo ours. */
bool phy_s3_ring_stale(void);
