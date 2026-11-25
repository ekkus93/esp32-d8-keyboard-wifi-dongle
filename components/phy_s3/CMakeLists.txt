# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(
    SRCS "src/phy_s3.c" "src/phy_s3_ring.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi esp_phy esp_event nvs_flash esp_hw_support esp_rom freertos heap soc)

# The wraps let phy_s3.c follow the PHY's own retunes and recalibrations.
target_link_options(${COMPONENT_LIB} INTERFACE
    "-T${CMAKE_CURRENT_LIST_DIR}/sram_guard.ld"
    "-Wl,--wrap=chip_v7_set_chan_ana"
    "-Wl,--wrap=set_rx_gain_cal_dc")
