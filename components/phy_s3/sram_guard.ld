/* SPDX-License-Identifier: GPL-3.0-only */
/* Fail the link if anything lands in the three modem SRAM banks.
 * From halo-esp32s3 (121344e), after ESPARGOS/esp-sdr's linker guards.
 */
ASSERT(_iram_end <= 0x403a0000, "S3 radio banks overlap IRAM")
ASSERT(_data_end <= 0x3fcb0000, "S3 radio banks overlap data")
ASSERT(_bss_end <= 0x3fcb0000, "S3 radio banks overlap BSS")
