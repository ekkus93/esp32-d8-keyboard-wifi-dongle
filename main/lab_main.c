/* SPDX-License-Identifier: GPL-3.0-only */
/* Lab firmware (board B): line console over USB Serial/JTAG for radio experiments.
 *
 * LAB BUILD ONLY: this firmware transmits. Owner approval 2026-11-25
 * (docs/DECISIONS.md). Normal receiver builds must not include these commands.
 *
 * Commands (one per line, replies start with OK or ERR):
 *   PING
 *   TUNE <mhz> [khz]                      retune + recalibrate (~3 ms)
 *   TXSEARCH <gain 0..10> <count> <interval_ms> [flow_hex]
 *       transmit copies of the D8 search frame (51 <flow> 01 00 00 00 00 0c 00 00 10)
 *       on the pairing access code, for TX-path verification with an independent receiver.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esb.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfsk.h"
#include "phy_s3.h"

#define LEAD_SAMPLES 400u      /* 10 us of silence before the preamble */
#define PREAMBLE_BITS 64u      /* 8 bytes; the keyboard uses ~12, the SDK dongle 8 */
#define MAX_GAIN 10u

static gfsk_t g_frame;   /* large: keep off the stack */
static unsigned cur_mhz = 2405;
static int cur_khz;

static void fill_frame(size_t first, uint32_t *out, size_t count, void *ctx)
{
    gfsk_fill((const gfsk_t *)ctx, first, out, count);
}

static void cmd_tune(char *args)
{
    char *end;
    long mhz = strtol(args, &end, 10);
    long khz = strtol(end, NULL, 10);
    if (mhz < 2400 || mhz > 2500 || khz < -999 || khz > 999) {
        printf("ERR range\n");
        return;
    }
    int64_t t = esp_timer_get_time();
    phy_s3_set_carrier((unsigned)mhz, (int)khz);
    cur_mhz = (unsigned)mhz;
    cur_khz = (int)khz;
    printf("OK TUNE %u %d %lld us\n", cur_mhz, cur_khz, (long long)(esp_timer_get_time() - t));
}

static void cmd_txsearch(char *args)
{
    unsigned gain = 0, count = 0, interval = 0, flow = 0x40;
    int n = sscanf(args, "%u %u %u %x", &gain, &count, &interval, &flow);
    if (n < 3 || gain > MAX_GAIN || count == 0 || count > 1000 || interval > 10000 || flow > 0xff) {
        printf("ERR args: TXSEARCH <gain 0..10> <count 1..1000> <interval_ms> [flow_hex]\n");
        return;
    }
    const uint8_t pay[11] = {0x51, (uint8_t)flow, 0x01, 0, 0, 0, 0, 0x0c, 0, 0, 0x10};
    static uint8_t bits[GFSK_MAX_BITS];
    size_t nbits = esb_build(bits, sizeof bits, PREAMBLE_BITS, &ESB_D8_PAIRING_ADDR, 0, 1, pay, sizeof pay);
    if (!nbits || LEAD_SAMPLES + nbits * GFSK_SPB > PHY_S3_TX_SAMPLES) {
        printf("ERR frame\n");
        return;
    }
    gfsk_prepare(&g_frame, bits, nbits, LEAD_SAMPLES);
    unsigned done = 0;
    for (unsigned k = 0; k < count; k++) {
        phy_s3_tx_result_t r = phy_s3_send(fill_frame, &g_frame, gain);
        (void)r;
        done++;
        if (interval) {
            vTaskDelay(pdMS_TO_TICKS(interval));
        }
    }
    printf("OK TXSEARCH sent=%u bits=%u samples=%u carrier=%u%+dkHz gain=%u flow=%02x\n", done, (unsigned)nbits,
           (unsigned)gfsk_length(&g_frame), cur_mhz, cur_khz, gain, flow);
}

void app_main(void)
{
    usb_serial_jtag_driver_config_t cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&cfg));
    usb_serial_jtag_vfs_use_driver();
    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    gfsk_init();
    phy_s3_init();
    phy_s3_set_carrier(cur_mhz, cur_khz);
    printf("D8LAB ready (lab build: transmits on command)\n");

    char line[128];
    for (;;) {
        if (!fgets(line, sizeof line, stdin)) {
            vTaskDelay(1);
            continue;
        }
        line[strcspn(line, "\r\n")] = 0;
        char *sp = strchr(line, ' ');
        char *args = sp ? sp + 1 : line + strlen(line);
        if (sp) {
            *sp = 0;
        }
        if (!strcmp(line, "PING")) {
            printf("OK PONG\n");
        } else if (!strcmp(line, "TUNE")) {
            cmd_tune(args);
        } else if (!strcmp(line, "TXSEARCH")) {
            cmd_txsearch(args);
        } else if (line[0]) {
            printf("ERR unknown %s\n", line);
        }
    }
}
