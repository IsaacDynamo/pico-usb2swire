#include <string.h>

#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/dma.h"

#include "config.h"
#include "swire.h"
#include "swire_encode.h"
#include "swire.pio.h"

#define SWS_MASK (1u << PIN_SWS)

unsigned char swire_addr_len = 3;
unsigned char swire_cmd_cpu_stop[6] = { 0x5a, 0x00, 0x06, 0x02, 0x00, 0x05 };
unsigned char swire_clk_div = SWIRE_CLK_DIV_DEFAULT;

static PIO  swpio = pio0;
static uint sm_tx, sm_rx;
static uint off_tx, off_rx;
static int  dma_tx;

typedef enum { BUS_IDLE, BUS_TX, BUS_RX } bus_state_t;
static bus_state_t bus_state = BUS_IDLE;

static uint32_t enc_buf[SWIRE_ENC_WORDS];

uint32_t swire_unit_ns(void) {
    return ((uint32_t)swire_clk_div * 1000u) / (EMU_CORE_HZ / 1000000u);
}

static void bus_idle(void) {
    pio_sm_set_enabled(swpio, sm_tx, false);
    pio_sm_set_enabled(swpio, sm_rx, false);
    pio_sm_set_pindirs_with_mask(swpio, sm_tx, 0, SWS_MASK);
    pio_sm_set_pins_with_mask(swpio, sm_tx, 0, SWS_MASK);
    bus_state = BUS_IDLE;
}

void swire_bus_release(void) {
    bus_idle();
}

static void bus_to_tx(void) {
    if (bus_state == BUS_TX) {
        pio_sm_set_enabled(swpio, sm_tx, false);
    } else {
        pio_sm_set_enabled(swpio, sm_rx, false);
        pio_sm_set_enabled(swpio, sm_tx, false);
        pio_sm_set_pins_with_mask(swpio, sm_tx, SWS_MASK, SWS_MASK);
        pio_sm_set_pindirs_with_mask(swpio, sm_tx, SWS_MASK, SWS_MASK);
    }
    pio_sm_clear_fifos(swpio, sm_tx);
    pio_sm_restart(swpio, sm_tx);
    pio_sm_clkdiv_restart(swpio, sm_tx);
    pio_sm_exec(swpio, sm_tx, pio_encode_jmp(off_tx));
    pio_sm_set_enabled(swpio, sm_tx, true);
    bus_state = BUS_TX;
}

static void bus_to_rx(void) {
    pio_sm_set_enabled(swpio, sm_tx, false);
    pio_sm_set_enabled(swpio, sm_rx, false);
    pio_sm_set_pindirs_with_mask(swpio, sm_rx, 0, SWS_MASK);
    pio_sm_set_pins_with_mask(swpio, sm_rx, 0, SWS_MASK);
    pio_sm_clear_fifos(swpio, sm_rx);
    pio_sm_restart(swpio, sm_rx);
    pio_sm_clkdiv_restart(swpio, sm_rx);
    pio_sm_exec(swpio, sm_rx, pio_encode_jmp(off_rx));
    pio_sm_set_enabled(swpio, sm_rx, true);
    bus_state = BUS_RX;
}

static void tx_wait_drain(void) {
    while (!pio_sm_is_tx_fifo_empty(swpio, sm_tx)) tight_loop_contents();
    busy_wait_us_32((swire_unit_ns() * 33u) / 1000u + 2u);
}

static void tx_run(const uint32_t *buf, uint32_t nwords, unsigned int repeat) {
    dma_channel_config c;

    if (!nwords || !repeat) return;

    bus_to_tx();

    c = dma_channel_get_default_config(dma_tx);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_dreq(&c, pio_get_dreq(swpio, sm_tx, true));
    dma_channel_configure(dma_tx, &c, &swpio->txf[sm_tx], buf, nwords, true);

    while (--repeat) {
        dma_channel_wait_for_finish_blocking(dma_tx);
        dma_channel_set_read_addr(dma_tx, buf, true);
    }
    dma_channel_wait_for_finish_blocking(dma_tx);
    tx_wait_drain();
}

static unsigned int rx_run(unsigned char *dst, unsigned int len) {
    unsigned int i = 0;
    uint32_t timeout_us;

    if (!len) return 0;

    timeout_us = 15u * (uint32_t)swire_clk_div + 50u;

    bus_to_rx();
    pio_sm_put(swpio, sm_rx, len - 1u);

    while (i < len) {
        absolute_time_t deadline = make_timeout_time_us(timeout_us);
        while (pio_sm_is_rx_fifo_empty(swpio, sm_rx)) {
            if (time_reached(deadline)) goto timeout;
        }
        dst[i++] = (unsigned char)(~pio_sm_get(swpio, sm_rx));
    }

    pio_sm_set_enabled(swpio, sm_rx, false);
    pio_sm_set_pindirs_with_mask(swpio, sm_rx, 0, SWS_MASK);
    bus_state = BUS_IDLE;
    return i;

timeout:
    pio_sm_set_enabled(swpio, sm_rx, false);
    pio_sm_set_pindirs_with_mask(swpio, sm_rx, 0, SWS_MASK);
    bus_state = BUS_IDLE;
    {
        absolute_time_t settle = make_timeout_time_us(64u * swire_unit_ns() / 1000u + 20u);
        while (!gpio_get(PIN_SWS) && !time_reached(settle)) tight_loop_contents();
    }
    return i;
}

void swire_write(unsigned char data, unsigned char ctrl) {
    sw_enc_t e;
    if (!(ctrl & FLD_SWIRE_WR)) return;
    sw_enc_begin(&e, enc_buf, SWIRE_ENC_WORDS);
    sw_enc_byte(&e, data, (ctrl & FLD_SWIRE_CMD) != 0);
    tx_run(enc_buf, sw_enc_end(&e), 1);
}

unsigned int swire_read(unsigned char *data, unsigned int len) {
    return rx_run(data, len);
}

static void enc_frame_head(sw_enc_t *e, unsigned int addr, unsigned char bid) {
    sw_enc_begin(e, enc_buf, SWIRE_ENC_WORDS);
    sw_enc_byte(e, swire_cmd_cpu_stop[0], true);
    if (swire_addr_len == 3) {
        sw_enc_byte(e, (uint8_t)(addr >> 16), false);
    }
    sw_enc_byte(e, (uint8_t)(addr >> 8), false);
    sw_enc_byte(e, (uint8_t)addr, false);
    sw_enc_byte(e, bid, false);
}

unsigned int swire_write_bytes(unsigned int addr, unsigned char *pdata, unsigned int len) {
    sw_enc_t e;
    unsigned int cnt = len;

    if (len > SWIRE_MAX_FRAME_BYTES - 6u) return 0;

    enc_frame_head(&e, addr, (unsigned char)(swire_cmd_cpu_stop[4] & 0x7f));
    while (cnt--) {
        sw_enc_byte(&e, *pdata++, false);
    }
    sw_enc_byte(&e, 0xff, true);
    tx_run(enc_buf, sw_enc_end(&e), 1);
    return len;
}

unsigned int swire_read_bytes(unsigned int addr, unsigned char *pdata, unsigned int len) {
    sw_enc_t e;
    unsigned int cnt;

    enc_frame_head(&e, addr, (unsigned char)((swire_cmd_cpu_stop[4] & 0x7f) | 0x80));
    tx_run(enc_buf, sw_enc_end(&e), 1);
    cnt = rx_run(pdata, len);
    swire_write(0xff, FLD_SWIRE_WR | FLD_SWIRE_CMD);
    return cnt;
}

unsigned int swire_fifo_write(unsigned int addr, unsigned char *pdata, unsigned int len) {
    unsigned int cnt;
    unsigned char tmp = 0x80;
    swire_write_bytes(0x00b3, &tmp, 1);
    cnt = swire_write_bytes(addr, pdata, len);
    tmp = 0x00;
    swire_write_bytes(0x00b3, &tmp, 1);
    return cnt;
}

unsigned int swire_fifo_read(unsigned int addr, unsigned char *pdata, unsigned int len) {
    unsigned int cnt;
    unsigned char tmp = 0x80;
    swire_write_bytes(0x00b3, &tmp, 1);
    cnt = swire_read_bytes(addr, pdata, len);
    tmp = 0x00;
    swire_write_bytes(0x00b3, &tmp, 1);
    return cnt;
}

void swire_set_clk_div(unsigned char div) {
    if (div == 0) div = 1;
    swire_clk_div = div;
    swpio->sm[sm_tx].clkdiv = (uint32_t)div << PIO_SM0_CLKDIV_INT_LSB;
    swpio->sm[sm_rx].clkdiv = (uint32_t)div << PIO_SM0_CLKDIV_INT_LSB;
    pio_sm_clkdiv_restart(swpio, sm_tx);
    pio_sm_clkdiv_restart(swpio, sm_rx);
}

void swire_activate(unsigned int count) {
    sw_enc_t e;
    unsigned int addr;
    uint32_t want, have;

    if (!count) return;

    addr = ((unsigned int)swire_cmd_cpu_stop[1] << 16)
         | ((unsigned int)swire_cmd_cpu_stop[2] << 8)
         |  (unsigned int)swire_cmd_cpu_stop[3];

    enc_frame_head(&e, addr, (unsigned char)(swire_cmd_cpu_stop[4] & 0x7f));
    sw_enc_byte(&e, swire_cmd_cpu_stop[5], false);
    sw_enc_byte(&e, 0xff, true);

    want = (uint32_t)swire_addr_len * SWIRE_UNITS_PER_BYTE + 285u;
    have = e.nunits + SW_ENC_TAIL_UNITS;
    if (want > have) sw_enc_idle(&e, want - have);

    tx_run(enc_buf, sw_enc_end(&e), count);
    bus_idle();
}

void swire_init(void) {
    sw_enc_init();

    sm_tx = (uint)pio_claim_unused_sm(swpio, true);
    sm_rx = (uint)pio_claim_unused_sm(swpio, true);
    off_tx = (uint)pio_add_program(swpio, &swire_tx_program);
    off_rx = (uint)pio_add_program(swpio, &swire_rx_program);

    gpio_pull_up(PIN_SWS);
    gpio_set_slew_rate(PIN_SWS, GPIO_SLEW_RATE_FAST);
    gpio_set_drive_strength(PIN_SWS, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_input_hysteresis_enabled(PIN_SWS, true);
    pio_gpio_init(swpio, PIN_SWS);

    swire_tx_program_init(swpio, sm_tx, off_tx, PIN_SWS);
    swire_rx_program_init(swpio, sm_rx, off_rx, PIN_SWS);
    swire_set_clk_div(SWIRE_CLK_DIV_DEFAULT);

    dma_tx = dma_claim_unused_channel(true);

    bus_idle();
}
