# SPDX-License-Identifier: GPL-3.0-only
idf_component_register(
    SRCS "lab_main.c"
    PRIV_REQUIRES phy_s3 gfsk esb esp_driver_usb_serial_jtag esp_timer)
