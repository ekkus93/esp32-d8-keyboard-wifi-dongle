/* SPDX-License-Identifier: GPL-3.0-only */
/* Adapted from halo-esp32s3 main/radio.c (121344e), which takes it from
 * ESPARGOS/esp-sdr and cyber-murmel/esp-sdr-zephyr; restructured for this
 * project. Every register write and private call, and their order, still
 * matches. Our own ring capture is in phy_s3_ring.c. Change nothing without
 * testing on real hardware.
 *
 * esp32-d8-receiver modifications (2026-11-25), from screenbar-halo-bridge 67deba5:
 * the fixed carrier define became a runtime carrier (phy_s3_set_carrier), so the
 * radio can follow the D8 keyboard's channels; phy_s3_send_settle makes the TX
 * settle time a parameter. Register sequences are unchanged.
 */
#include "phy_s3.h"
#include "phy_s3_hw.h"

#include <stdatomic.h>
#include <sys/lock.h>

#include "esp_cpu.h"
#include "esp_event.h"
#include "esp_private/phy.h"
#include "esp_rom_sys.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "heap_memory_layout.h"
#include "nvs_flash.h"
#include "soc/soc.h"

SOC_RESERVE_MEMORY_REGION(BANK_BASE, BANK_BASE + 3u * BANK_SIZE, phy_s3_banks);

_Static_assert(PHY_S3_CAPTURE_SAMPLES == BANK_WORDS, "a capture fills exactly one bank");

/* The DAC keeps its address counter when we hand it the second bank, so bank 1
 * must hold the waveform at the same offsets. We switch with 64 samples to spare.
 */
#define SWITCH_AT (BANK_WORDS - 64u - 2048u)

#define CHANNEL_MHZ   2412   /* Wi-Fi channel 1: the PHY needs a real channel to tune from */
/* Carrier, reached with an RF offset from CHANNEL_MHZ. Runtime-settable (phy_s3_set_carrier). */
static unsigned carrier_mhz = 2405;
static int carrier_khz = 0;
#define SETTLE_US     3000

#define DAC_CYCLES_PER_SAMPLE 6u                        /* 240 MHz / 40 MS/s */
#define ADC_TIMEOUT_CYCLES    (2000u * CYCLES_PER_US)   /* twice the capture length */

extern void rom_pbus_xpd_tx_on(unsigned);
extern void rom_pbus_xpd_tx_off(void);
extern void stop_tx_tone(unsigned);
extern void force_tx_gain(unsigned, unsigned, int);
extern void force_txon_mode(unsigned, unsigned, unsigned);
extern void set_chanfreq(unsigned, unsigned);
extern void set_rf_freq_offset(unsigned, unsigned, int);
extern void set_rx_gain_table(unsigned, unsigned);
extern unsigned rom_chip_i2c_readReg(unsigned, unsigned, unsigned);
extern void rom_chip_i2c_writeReg(unsigned, unsigned, unsigned, unsigned);
extern unsigned char phy_param[];

/* The PHY recalibrates on its own schedule; each pass bumps this, and the next
 * listen redoes our fixed-carrier calibration.
 */
static atomic_uint dc_generation;
static unsigned calibrated_generation;
static bool calibrated;
static bool measuring;   /* our own calibration is running */
static bool rx_ready;

portMUX_TYPE phy_s3_mux = portMUX_INITIALIZER_UNLOCKED;

/* Uncalibrated register values for gain index 0..10; upstream only ever tested 8. */
static const unsigned tx_gains[PHY_S3_GAIN_MAX + 1] = {0, 16, 32, 48, 49, 81, 82, 115, 87, 119, 107};

/* The LO path esp-sdr uses: bit 4 of analog block 0x65 register 1 cleared. */
void phy_s3_select_lo(void)
{
    unsigned old = rom_chip_i2c_readReg(0x65, 1, 0);
    if (old & 0x10u) {
        rom_chip_i2c_writeReg(0x65, 1, 0, old & ~0x10u);
    }
}

/* Linker wraps (see CMakeLists.txt): keep the PHY on our carrier whenever it
 * retunes during our calibration, and notice every DC calibration it does.
 */
extern void __real_chip_v7_set_chan_ana(unsigned);
void __wrap_chip_v7_set_chan_ana(unsigned channel)
{
    __real_chip_v7_set_chan_ana(channel);
    if (measuring) {
        set_rf_freq_offset(0, carrier_mhz, carrier_khz);
        phy_s3_select_lo();
    }
}

extern void __real_set_rx_gain_cal_dc(uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t,
                                      uintptr_t, uintptr_t, uintptr_t);
void __wrap_set_rx_gain_cal_dc(uintptr_t a, uintptr_t b, uintptr_t c, uintptr_t d, uintptr_t e, uintptr_t f,
                               uintptr_t g, uintptr_t h, uintptr_t i, uintptr_t j)
{
    __real_set_rx_gain_cal_dc(a, b, c, d, e, f, g, h, i, j);
    atomic_fetch_add(&dc_generation, 1);
}

static void calibrate_rx(void)
{
    _lock_t lock = phy_get_lock();
    _lock_acquire(&lock);
    measuring = true;
    REG_CLR_BIT(RX_CAL_REG, RX_CAL_BYPASS_BIT);
    set_rf_freq_offset(0, carrier_mhz, carrier_khz);
    phy_s3_select_lo();
    *(uint32_t *)(phy_param + PHY_PARAM_RX_OFFSET) &= ~0x200u;
    set_rx_gain_table(carrier_mhz, 0);
    measuring = false;
    calibrated_generation = atomic_load(&dc_generation);
    calibrated = true;
    _lock_release(&lock);
}

void phy_s3_prepare_rx(void)
{
    bool stale = !calibrated || atomic_load(&dc_generation) != calibrated_generation;
    if (rx_ready && !stale) {
        return;
    }
    if (stale) {
        calibrate_rx();
    }
    phy_s3_select_lo();
    set_chanfreq(CHANNEL_MHZ, 0);
    set_rf_freq_offset(0, carrier_mhz, carrier_khz);
    stop_tx_tone(1);
    rom_pbus_workmode();
    rom_pbus_xpd_tx_off();
    rom_pbus_xpd_rx_on(1);
    rom_set_rxclk_en(1);
    force_rx_gain(0, RX_GAIN_INDEX, 0);
    phy_s3_select_lo();
    esp_rom_delay_us(SETTLE_US);
    rx_ready = true;
}

/* Bank switch timing is done by counting CPU cycles, so interrupts stay masked. */
static uint32_t IRAM_ATTR play_banks(void)
{
    uint32_t saved = REG_READ(SRAM_OWNER_REG);
    uint32_t ctrl = CTRL_ENABLE | BANK_WORDS;

    portENTER_CRITICAL(&phy_s3_mux);
    REG_WRITE(SRAM_OWNER_REG, (saved & ~15u) | 1u);
    REG_WRITE(DAC_CTRL_REG, 0);
    REG_WRITE(DAC_CTRL_REG, ctrl);
    REG_WRITE(DAC_CTRL_REG, ctrl | CTRL_START);
    uint32_t t = esp_cpu_get_cycle_count();
    while ((uint32_t)(esp_cpu_get_cycle_count() - t) < SWITCH_AT * DAC_CYCLES_PER_SAMPLE) {
    }
    REG_WRITE(SRAM_OWNER_REG, (saved & ~15u) | 2u);
    while ((uint32_t)(esp_cpu_get_cycle_count() - t) < PHY_S3_TX_SAMPLES * DAC_CYCLES_PER_SAMPLE) {
    }
    REG_WRITE(DAC_CTRL_REG, 0);
    uint32_t tx_end = esp_cpu_get_cycle_count();
    REG_WRITE(ADC_CTRL_REG, 0);
    REG_WRITE(SRAM_OWNER_REG, saved);
    portEXIT_CRITICAL(&phy_s3_mux);
    return tx_end;
}

/* Raw ADC capture into bank 0. *start gets the cycle count when it began. */
static bool capture_bank0(uint32_t *start)
{
    uint32_t saved = REG_READ(SRAM_OWNER_REG);
    uint32_t ctrl = CTRL_ENABLE | ADC_MODE_RAW | BANK_WORDS;

    portENTER_CRITICAL(&phy_s3_mux);
    REG_WRITE(ADC_CTRL_REG, 0);
    REG_WRITE(ADC_CONFIG_REG, ADC_CONFIG_VALUE);
    REG_WRITE(SRAM_OWNER_REG, (saved & ~15u) | 1u);
    REG_WRITE(ADC_CTRL_REG, ctrl);
    REG_WRITE(ADC_CTRL_REG, ctrl | CTRL_START);
    uint32_t t = esp_cpu_get_cycle_count();
    *start = t;
    REG_WRITE(ADC_CTRL_REG, ctrl);
    while (!(REG_READ(ADC_CTRL_REG) & CTRL_DONE) && (uint32_t)(esp_cpu_get_cycle_count() - t) < ADC_TIMEOUT_CYCLES) {
    }
    bool done = (REG_READ(ADC_CTRL_REG) & CTRL_DONE) != 0;
    REG_WRITE(ADC_CTRL_REG, 0);
    REG_WRITE(SRAM_OWNER_REG, saved);
    portEXIT_CRITICAL(&phy_s3_mux);
    return done;
}

void phy_s3_init(void)
{
    /* Wi-Fi is started only to power and calibrate the PHY; it never joins anything. */
    esp_err_t e = nvs_flash_init();
    if (e == ESP_ERR_NVS_NO_FREE_PAGES || e == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        e = nvs_flash_init();
    }
    ESP_ERROR_CHECK(e);
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));
    ESP_ERROR_CHECK(esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE));
    phy_s3_prepare_rx();
}

void phy_s3_load(phy_s3_fill_fn fill, void *ctx)
{
    /* Bank 1 holds samples SWITCH_AT - 64 .. SWITCH_AT - 64 + BANK_WORDS at their
     * offset modulo the bank size, so it splits into a tail part and a wrapped part.
     */
    const size_t tail = SWITCH_AT - 64u;
    const size_t wrapped = PHY_S3_TX_SAMPLES - BANK_WORDS;
    fill(0, BANK(0), BANK_WORDS, ctx);
    fill(tail, BANK(1) + tail, BANK_WORDS - tail, ctx);
    fill(BANK_WORDS, BANK(1), wrapped, ctx);
    for (size_t n = wrapped; n < tail; n++) {
        BANK(1)[n] = 0;
    }
}

phy_s3_tx_result_t phy_s3_send(phy_s3_fill_fn fill, void *ctx, unsigned gain)
{
    return phy_s3_send_settle(fill, ctx, gain, SETTLE_US);
}

phy_s3_tx_result_t phy_s3_send_settle(phy_s3_fill_fn fill, void *ctx, unsigned gain, unsigned settle_us)
{
    phy_s3_tx_result_t r = {0};
    if (gain > PHY_S3_GAIN_MAX) {
        return r;
    }
    phy_s3_load(fill, ctx);
    rom_pbus_workmode();
    rom_pbus_xpd_rx_on(0);
    rom_pbus_xpd_tx_on(1);
    force_tx_gain(tx_gains[gain], 0, 0);
    phy_s3_select_lo();
    force_txon_mode(1, 0, 0);
    esp_rom_delay_us(settle_us);

    uint32_t tx_end = play_banks();

    /* No retune and no settle here: the ACK is only ~184 us away. */
    force_txon_mode(0, 0, 0);
    rom_pbus_xpd_tx_off();
    rom_pbus_workmode();
    rom_pbus_xpd_rx_on(1);
    rom_set_rxclk_en(1);
    force_rx_gain(0, RX_GAIN_INDEX, 0);
    phy_s3_select_lo();
    uint32_t start;
    r.captured = capture_bank0(&start);
    r.turnaround_us = (start - tx_end) / CYCLES_PER_US;
    rx_ready = true;
    return r;
}

void phy_s3_reset_rx(void)
{
    rx_ready = false;
    phy_s3_prepare_rx();
}

bool phy_s3_listen(void)
{
    phy_s3_prepare_rx();
    rom_pbus_workmode();
    rom_pbus_xpd_rx_on(1);
    rom_set_rxclk_en(1);
    force_rx_gain(0, RX_GAIN_INDEX, 0);
    phy_s3_select_lo();
    uint32_t start;
    return capture_bank0(&start);
}

const uint32_t *phy_s3_capture(void)
{
    return BANK(0);
}

void *phy_s3_spare(void)
{
    return BANK(2);
}

bool phy_s3_dc_stale(void)
{
    return atomic_load(&dc_generation) != calibrated_generation;
}

void phy_s3_set_carrier(unsigned mhz, int khz)
{
    carrier_mhz = mhz;
    carrier_khz = khz;
    calibrated = false;   /* redo the fixed-carrier calibration on the new carrier */
    rx_ready = false;
    phy_s3_prepare_rx();
}
