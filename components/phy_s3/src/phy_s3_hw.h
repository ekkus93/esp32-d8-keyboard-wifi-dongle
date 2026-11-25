/* SPDX-License-Identifier: GPL-3.0-only */
/* Inside phy_s3 only: the hardware map and the few internals the ring capture
 * shares with the upstream-derived code in phy_s3.c. The addresses come from
 * halo-esp32s3 (121344e) and ESPARGOS/esp-sdr.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "phy_s3.h"

/* Three 64 KB modem SRAM banks: 0 and 1 hold the DAC waveform, the ADC captures into 0. */
#define BANK_BASE  0x3fcb0000u
#define BANK_SIZE  0x10000u
#define BANK_WORDS 16380u
#define BANK(n)    ((uint32_t *)(BANK_BASE + (n) * BANK_SIZE))

#define SRAM_OWNER_REG        0x600c101cu
#define DAC_CTRL_REG          0x60033d64u
#define ADC_CTRL_REG          0x60033d5cu
#define ADC_CONFIG_REG        0x60033d90u
#define ADC_CONFIG_VALUE      0x000c2040u
#define RX_CAL_REG            0x6001c02cu
#define RX_CAL_BYPASS_BIT     (1u << 23)
#define PHY_PARAM_RX_OFFSET   288   /* bit 9 here must be clear before the gain table */

#define CTRL_ENABLE   0x80000000u
#define CTRL_START    (1u << 19)
#define CTRL_DONE     (1u << 18)
#define ADC_MODE_RAW  (1u << 16)

#define RX_GAIN_INDEX 40
#define CYCLES_PER_US PHY_S3_CYCLES_PER_US

/* The DAC's 6 and the ring's 15 cycles per sample are counted at 240 MHz. */
_Static_assert(CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ == 240, "phy_s3 needs the CPU at 240 MHz");

extern void rom_pbus_workmode(void);
extern void rom_pbus_xpd_rx_on(unsigned);
extern void rom_set_rxclk_en(unsigned);
extern void force_rx_gain(unsigned, unsigned, unsigned);

extern portMUX_TYPE phy_s3_mux;

void phy_s3_select_lo(void);
void phy_s3_prepare_rx(void);
/* The PHY ran a DC calibration since our last one. */
bool phy_s3_dc_stale(void);
