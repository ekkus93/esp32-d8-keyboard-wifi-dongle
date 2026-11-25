/* SPDX-License-Identifier: GPL-3.0-only */
/* Gap-free capture with the ADC's ring mode, which esp-sdr and esp-sdr-zephyr
 * document too; we found it on our own by filling the banks with markers. Ours
 * is the timing: each bank flip comes from the cycle counter, not a write
 * index, with markers in the tail to find where it lands.
 */
#include "phy_s3.h"
#include "phy_s3_hw.h"

#include "esp_cpu.h"
#include "soc/soc.h"

#define ADC_MODE_RING   (1u << 14)   /* never DONE; the write address wraps every 16384 words */
#define RING_WORDS      PHY_S3_RING_SAMPLES
#define ADC_CYCLES      PHY_S3_RING_CYCLES_PER_SAMPLE   /* from the same crystal, so no drift */
#define LAP_CYCLES      (RING_WORDS * ADC_CYCLES)
#define FLIP_EARLY      64u          /* the ADC's start wanders ~+-25 samples per run */
#define TAIL_MARKED     256u         /* where to look for the flip */
#define MARKER          0xffffffffu  /* the ADC only writes 20 bits */
#define MASKED_CYCLES   (20u * CYCLES_PER_US)   /* interrupts off only for the end of the wait */

_Static_assert(PHY_S3_RING_SAMPLES * 4u == BANK_SIZE, "a lap fills a bank exactly");
_Static_assert(PHY_S3_SPARE_BYTES == BANK_SIZE, "the spare is a whole bank");

static uint32_t ring_owner;   /* SRAM_OWNER_REG before the ring took the banks */
static bool running;          /* stop restores ring_owner, which is only valid once start has read it */

static uint32_t owner(unsigned bank)
{
    return (ring_owner & ~15u) | (1u << bank);
}

bool phy_s3_ring_start(phy_s3_ring_t *ring, phy_s3_chunk_t *first)
{
    /* Same receiver setup as phy_s3_listen. */
    phy_s3_prepare_rx();
    rom_pbus_workmode();
    rom_pbus_xpd_rx_on(1);
    rom_set_rxclk_en(1);
    force_rx_gain(0, RX_GAIN_INDEX, 0);
    phy_s3_select_lo();

    /* The first flip lands where the ADC's start jitter puts it. Bank 0 keeps
     * the marks past that point, which tells us where every later flip lands too.
     */
    uint32_t *b0 = BANK(0);
    for (size_t j = RING_WORDS - TAIL_MARKED; j < RING_WORDS; j++) {
        b0[j] = MARKER;
    }

    ring_owner = REG_READ(SRAM_OWNER_REG);
    running = true;
    uint32_t ctrl = CTRL_ENABLE | ADC_MODE_RAW | ADC_MODE_RING | BANK_WORDS;
    portENTER_CRITICAL(&phy_s3_mux);
    REG_WRITE(ADC_CTRL_REG, 0);
    REG_WRITE(ADC_CONFIG_REG, ADC_CONFIG_VALUE);
    REG_WRITE(SRAM_OWNER_REG, owner(0));
    REG_WRITE(ADC_CTRL_REG, ctrl);
    REG_WRITE(ADC_CTRL_REG, ctrl | CTRL_START);
    uint32_t t = esp_cpu_get_cycle_count();
    REG_WRITE(ADC_CTRL_REG, ctrl);
    uint32_t at = t + (RING_WORDS - FLIP_EARLY) * ADC_CYCLES;
    while ((int32_t)(at - esp_cpu_get_cycle_count()) > 0) {
    }
    REG_WRITE(SRAM_OWNER_REG, owner(1));
    portEXIT_CRITICAL(&phy_s3_mux);

    size_t flip = RING_WORDS - TAIL_MARKED;
    while (flip < RING_WORDS && b0[flip] != MARKER) {
        flip++;
    }
    if (flip == RING_WORDS - TAIL_MARKED || flip == RING_WORDS) {
        phy_s3_ring_stop();   /* flip outside the marked window: timing is off */
        return false;
    }
    ring->flip = flip;
    ring->bank = 1;
    ring->next = at + LAP_CYCLES;
    *first = (phy_s3_chunk_t){.samples = b0, .from = 0, .to = flip};
    return true;
}

bool phy_s3_ring_next(phy_s3_ring_t *ring, phy_s3_chunk_t *chunk)
{
    uint32_t at = ring->next;
    while ((int32_t)(at - esp_cpu_get_cycle_count()) > (int32_t)MASKED_CYCLES) {
    }
    portENTER_CRITICAL(&phy_s3_mux);
    /* Even a few cycles late would move the flip off its offset, so give up instead. */
    bool late = (int32_t)(at - esp_cpu_get_cycle_count()) <= 0;
    while ((int32_t)(at - esp_cpu_get_cycle_count()) > 0) {
    }
    if (!late) {
        REG_WRITE(SRAM_OWNER_REG, owner(ring->bank ^ 1u));
    }
    portEXIT_CRITICAL(&phy_s3_mux);
    if (late) {
        phy_s3_ring_stop();
        return false;
    }
    *chunk = (phy_s3_chunk_t){.samples = BANK(ring->bank), .from = ring->flip, .to = ring->flip};
    ring->bank ^= 1u;
    ring->next = at + LAP_CYCLES;
    return true;
}

void phy_s3_ring_stop(void)
{
    if (!running) {
        return;
    }
    running = false;
    portENTER_CRITICAL(&phy_s3_mux);
    REG_WRITE(ADC_CTRL_REG, 0);
    REG_WRITE(SRAM_OWNER_REG, ring_owner);
    portEXIT_CRITICAL(&phy_s3_mux);
}

/* No PHY lock, same as before every one-shot capture: waiting on it could miss a flip. */
void phy_s3_ring_regain(void)
{
    force_rx_gain(0, RX_GAIN_INDEX, 0);
    phy_s3_select_lo();
}

bool phy_s3_ring_stale(void)
{
    return phy_s3_dc_stale();
}
