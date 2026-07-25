#include "tusb.h"
#include "pico/stdlib.h"

#include "config.h"
#include "proto.h"
#include "cdc_link.h"

static volatile bool     blk_ready;
static volatile bool     blk_partial;
static volatile uint32_t last_rx_us;

void cdc_link_init(void) {
    blk_ready = false;
    blk_partial = false;
    urxb.len = 0;
    tusb_init();
}

void tud_cdc_rx_cb(uint8_t itf) {
    uint32_t avail, n;
    (void)itf;

    if (blk_ready) return;

    avail = tud_cdc_available();
    if (!avail) return;

    if (urxb.len + avail > USB_MAX_BLK_SIZE) {
        tud_cdc_read_flush();
        urxb.len = 0;
        blk_partial = false;
        return;
    }

    n = tud_cdc_read(&urxb.uc[urxb.len], avail);
    urxb.len += n;
    last_rx_us = time_us_32();

    if (n & 63u) {
        blk_ready = true;
        blk_partial = false;
    } else {
        blk_partial = true;
    }
}

void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts) {
    (void)itf; (void)dtr; (void)rts;
    // TlsrPgm.py drops DTR and RTS right after opening the port.
    cdc_link_on_host_reset();
}

void cdc_link_task(void) {
    tud_task();

    if (!blk_ready && blk_partial && urxb.len &&
        (uint32_t)(time_us_32() - last_rx_us) >= USB_RX_IDLE_TIMEOUT_US) {
        /* length was an exact multiple of 64. */
        blk_ready = true;
        blk_partial = false;
    }
}

bool cdc_link_block_ready(void) {
    return blk_ready;
}

bool cdc_link_rx_started(void) {
    return urxb.len != 0;
}

void cdc_link_block_done(void) {
    urxb.len = 0;
    blk_ready = false;
    blk_partial = false;
}

void cdc_link_reset(void) {
    tud_cdc_read_flush();
    tud_cdc_write_clear();
    urxb.len = 0;
    blk_ready = false;
    blk_partial = false;
}

void cdc_link_send(const uint8_t *data, uint32_t len) {
    uint32_t sent = 0;

    while (sent < len) {
        uint32_t n = tud_cdc_write(data + sent, len - sent);
        sent += n;
        if (sent < len || n == 0) {
            tud_cdc_write_flush();
            tud_task();
            if (!tud_cdc_connected()) return;
        }
    }
    tud_cdc_write_flush();
}
