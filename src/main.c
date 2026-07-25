#include <string.h>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/clocks.h"

#include "config.h"
#include "proto.h"
#include "crc16.h"
#include "swire.h"
#include "swire_flash.h"
#include "cdc_link.h"

typedef enum {
    FLG_CMD_DONE = 0,
    FLG_CMD_SWS_PRINT,
    FLG_CMD_WAIT_RESP
} E_FLG_CMD_t;

static volatile E_FLG_CMD_t flg_wait_rx;

static sws_pintf_buf_t sws_pr_txb;

static struct {
    uint32_t len;
    uint32_t addr;
    uint8_t  data[4];
} wrsp_buf;

static unsigned char new_uart_baud = 1;

io_buf_t urxb;
io_buf_t utxb;

static void task_sws_print(unsigned int argv, sws_pintf_buf_t *p) {
    struct {
        unsigned char id;
        unsigned char len;
    } head;
    if (swire_read_bytes(argv, (unsigned char *)&head, sizeof(head)) == sizeof(head)) {
        if (head.id == 0x55) {
            if (head.len) {
                if (head.len <= 253) {
                    if (head.len == swire_read_bytes(argv, (unsigned char *)p, head.len + 2) - 2
                        && p->id == head.id
                        && p->len == head.len) {
                        head.len = 0;
                        swire_write_bytes(argv + 1, &head.len, 1);
                    } else
                        p->len = 0;
                } else {
                    head.len = 0;
                    swire_write_bytes(argv + 1, &head.len, 1);
                    p->len = 0;
                }
            }
        } else
            p->len = 0;
    }
}

void cdc_link_on_host_reset(void) {
    utxb.len = 0;
    flg_wait_rx = FLG_CMD_DONE;
    cdc_link_reset();
    swire_bus_release();
}

static void board_init(void) {
    set_sys_clock_khz(SWIRE_SYS_CLK_KHZ, true);

    crc16_init();

    gpio_init(PIN_RESET);
    gpio_disable_pulls(PIN_RESET);
    gpio_put(PIN_RESET, 0);
    gpio_set_dir(PIN_RESET, GPIO_IN);

    gpio_init(PIN_POWER);
    gpio_disable_pulls(PIN_POWER);
    gpio_put(PIN_POWER, 1);
    gpio_set_dir(PIN_POWER, GPIO_OUT);
    gpio_set_drive_strength(PIN_POWER, GPIO_DRIVE_STRENGTH_12MA);

    swire_init();
}

static inline void set_gpio_reset(void) { gpio_set_dir(PIN_RESET, GPIO_IN); }
static inline void clr_gpio_reset(void) { gpio_put(PIN_RESET, 0); gpio_set_dir(PIN_RESET, GPIO_OUT); }
static inline void set_gpio_power(void) { gpio_put(PIN_POWER, 1); }
static inline void clr_gpio_power(void) { gpio_put(PIN_POWER, 0); }

int main(void) {
    union {
        unsigned int  ud;
        unsigned char uc[4];
    } tmp = { .ud = 0 };
    unsigned int rxlen = 0;
    unsigned int argv = 0;
    uint16_t crc16;

    board_init();
    cdc_link_init();

    while (1) {
        cdc_link_task();

        while (flg_wait_rx) {
            while (flg_wait_rx == FLG_CMD_SWS_PRINT) {
                if (sws_pr_txb.len == 0) {
                    task_sws_print(argv, &sws_pr_txb);
                }
                if (sws_pr_txb.len) {
                    memcpy(utxb.uc, sws_pr_txb.data, sws_pr_txb.len);
                    utxb.len = sws_pr_txb.len;
                    cdc_link_send(utxb.uc, utxb.len);
                    utxb.len = 0;
                    sws_pr_txb.len = 0;
                }
                cdc_link_task();
                if (cdc_link_rx_started()) {
                    flg_wait_rx = FLG_CMD_DONE;
                    break;
                }
            }
            while (flg_wait_rx == FLG_CMD_WAIT_RESP) {
                utxb.pkt.head.count = swire_read_bytes(argv, utxb.pkt.data, 4);
                if (utxb.pkt.head.count == 4) {
                    if (wrsp_buf.len)
                        swire_write_bytes(wrsp_buf.addr, wrsp_buf.data, wrsp_buf.len);
                    flg_wait_rx = FLG_CMD_DONE;
                    utxb.len = sizeof(utxb.pkt.head) + 4;
                    crc16 = crc16_modbus(utxb.uc, utxb.len);
                    utxb.uc[utxb.len++] = (uint8_t)crc16;
                    utxb.uc[utxb.len++] = (uint8_t)(crc16 >> 8);
                    cdc_link_send(utxb.uc, utxb.len);
                    utxb.len = 0;
                }
                cdc_link_task();
                if (cdc_link_rx_started()) {
                    flg_wait_rx = FLG_CMD_DONE;
                    break;
                }
            }
        }

        if (cdc_link_block_ready()) {
            rxlen = urxb.len;
            utxb.len = sizeof(utxb.pkt.head);
            utxb.pkt.head.ud = urxb.pkt.head.cmd;
            if (rxlen < sizeof(urxb.pkt.head) + 2) {
                utxb.pkt.head.err = ERR_LEN;
            } else {
                crc16 = crc16_modbus(urxb.uc, rxlen);
                if (crc16 != 0) {
                    utxb.pkt.head.cmd |= ERR_CRC;
                } else {
                    urxb.uc[rxlen] = 0;
                    argv = urxb.pkt.head.b.argv;
                    rxlen -= sizeof(urxb.pkt.head) + 2;
                    switch (urxb.pkt.head.cmd) {
                        case CMD_SWIRE_READ:
                            if (rxlen == 0
                                || urxb.pkt.rd.len > sizeof(utxb.pkt.data))
                                urxb.pkt.rd.len = sizeof(utxb.pkt.data);
                            utxb.pkt.head.count = swire_read_bytes(argv, utxb.pkt.data, urxb.pkt.rd.len);
                            if (utxb.pkt.head.count != urxb.pkt.rd.len)
                                utxb.pkt.head.err = ERR_READ;
                            utxb.len += utxb.pkt.head.count;
                            break;

                        case CMD_SWIRE_WRITE:
                            if (rxlen == 0
                                || rxlen > sizeof(urxb.pkt.data))
                                utxb.pkt.head.err = ERR_LEN;
                            else
                                utxb.pkt.head.count = swire_write_bytes(argv, urxb.pkt.data, rxlen);
                            break;

                        case CMD_SWIRE_AREAD:
                            tmp.uc[0] = (unsigned char)argv;
                            if (rxlen == 0
                                || urxb.pkt.rd.len > 255)
                                urxb.pkt.rd.len = 256;
                            if (urxb.pkt.rd.len > 1) {
                                tmp.uc[2] = 0x50;
                                swire_write_bytes(0x00b8, tmp.uc, 3);
                                utxb.pkt.head.count = swire_fifo_read(0x00b9, utxb.pkt.data, urxb.pkt.rd.len);
                                if (utxb.pkt.head.count != urxb.pkt.rd.len)
                                    utxb.pkt.head.err = ERR_READ;
                                utxb.len += utxb.pkt.head.count;
                            } else {
                                tmp.uc[2] = FLD_ANA_START;
                                swire_write_bytes(0x00b8, tmp.uc, 3);
                                tmp.ud = 7;
                                while (1) {
                                    if (swire_read_bytes(0x00b9, utxb.pkt.data, 2) != 2) {
                                        utxb.pkt.head.err = ERR_READ;
                                        break;
                                    }
                                    if ((utxb.pkt.data[1] & (FLD_ANA_BUSY << 8)) == 0) {
                                        utxb.len++;
                                        utxb.pkt.head.count = 1;
                                        break;
                                    }
                                    if (tmp.ud--) {
                                        utxb.pkt.head.err = ERR_BUSY;
                                        break;
                                    }
                                }
                            }
                            tmp.ud = 0;
                            swire_write_bytes(0x00ba, tmp.uc, 1);
                            break;

                        case CMD_SWIRE_AWRITE:
                            if (rxlen == 0
                                || rxlen > 256)
                                utxb.pkt.head.err = ERR_LEN;
                            else {
                                unsigned char *p = urxb.pkt.data;
                                tmp.uc[0] = (unsigned char)argv;
                                tmp.uc[1] = *p++;
                                tmp.uc[3] = 0;
                                if (rxlen > 1) {
                                    swire_write_bytes(0x00b8, tmp.uc, 1);
                                    tmp.uc[2] = 0x70;
                                    while (rxlen--) {
                                        swire_write_bytes(0x00b9, &tmp.uc[1], 2);
                                        tmp.uc[1] = *p++;
                                        utxb.pkt.head.count++;
                                    }
                                } else {
                                    tmp.uc[2] = FLD_ANA_START;
                                    swire_write_bytes(0x00b8, tmp.uc, 3);
                                    utxb.pkt.head.count = 1;
                                }
                                swire_write_bytes(0x00ba, &tmp.uc[3], 1);
                            }
                            break;

                        case CMD_FLASH_READ:
                            if (rxlen == 0
                                || urxb.pkt.rd.len > sizeof(utxb.pkt.data))
                                urxb.pkt.rd.len = sizeof(utxb.pkt.data);
                            rxlen = urxb.pkt.rd.len;
                            utxb.pkt.head.count = read_flash(argv, utxb.pkt.data, rxlen);
                            if (utxb.pkt.head.count != rxlen)
                                utxb.pkt.head.err = ERR_READ;
                            utxb.len += utxb.pkt.head.count;
                            break;

                        case CMD_FLASH_WRITE:
                            if (rxlen == 0
                                || rxlen > 256)
                                utxb.pkt.head.err = ERR_LEN;
                            else {
                                swire_fcmd(FLASH_WRITE_ENABLE_CMD);
                                _swire_fcmd_faddr(FLASH_WRITE_CMD, argv);
                                if (rxlen == 1)
                                    utxb.pkt.head.count = swire_write_bytes(0x000c, urxb.pkt.data, rxlen);
                                else
                                    utxb.pkt.head.count = swire_fifo_write(0x000c, urxb.pkt.data, rxlen);
                                _swire_set_fcs_hi();
                                // orig has a "@TODO Flash flags ready?" idk what this means homeboy
                            }
                            break;

                        case CMD_FLASH_SECT_ERASE:
                            swire_fcmd(FLASH_WRITE_ENABLE_CMD);
                            _swire_fcmd_faddr(FLASH_SECT_ERASE_CMD, argv);
                            // @TODO Flash flags ready?
                            _swire_set_fcs_hi();
                            break;

                        case CMD_FLASH_ALL_ERASE:
                            swire_fcmd(FLASH_WRITE_ENABLE_CMD);
                            swire_fcmd(FLASH_CHIP_ERASE_CMD);
                            // @TODO Flash flags ready?
                            _swire_set_fcs_hi();
                            break;

                        case CMD_SWIRE_FIFO_READ:
                            if (rxlen == 0
                                || urxb.pkt.rd.len > sizeof(utxb.pkt.data))
                                urxb.pkt.rd.len = sizeof(utxb.pkt.data);
                            utxb.pkt.head.count = swire_fifo_read(argv, utxb.pkt.data, urxb.pkt.rd.len);
                            if (utxb.pkt.head.count != urxb.pkt.rd.len)
                                utxb.pkt.head.err = ERR_READ;
                            utxb.len += utxb.pkt.head.count;
                            break;

                        case CMD_SWIRE_FIFO_FWRITE:
                            if (rxlen == 0
                                || rxlen > sizeof(urxb.pkt.data))
                                utxb.pkt.head.err = ERR_LEN;
                            else
                                utxb.pkt.head.count = swire_fifo_write(argv, urxb.pkt.data, rxlen);
                            break;

                        case CMD_FLASH_GET_JEDEC_ID:
                            tmp.ud = FLASH_GET_JEDEC_ID;
                            _swire_set_fcs_low();
                            swire_write_bytes(0x000c, tmp.uc, 1);
                            tmp.ud = (FLD_MASTER_SPI_RD | FLD_MASTER_SPI_SDO) << 8;
                            swire_write_bytes(0x000c, tmp.uc, 2);
                            rxlen = 3;
                            utxb.pkt.head.count = swire_fifo_read(0x000c, utxb.pkt.data, rxlen);
                            _swire_set_fcs_hi();
                            if (utxb.pkt.head.count != rxlen)
                                utxb.pkt.head.err = ERR_READ;
                            utxb.len += utxb.pkt.head.count;
                            break;

                        case CMD_FLASH_GET_STATUS:
                            tmp.ud = FLASH_READ_STATUS_CMD;
                            _swire_set_fcs_low();
                            swire_write_bytes(0x000c, tmp.uc, 1);
                            tmp.ud = (FLD_MASTER_SPI_RD | FLD_MASTER_SPI_SDO) << 8;
                            swire_write_bytes(0x000c, tmp.uc, 2);
                            rxlen = 1;
                            utxb.pkt.head.count = swire_read_bytes(0x000c, utxb.pkt.data, rxlen);
                            _swire_set_fcs_hi();
                            if (utxb.pkt.head.count != rxlen)
                                utxb.pkt.head.err = ERR_READ;
                            utxb.len += utxb.pkt.head.count;
                            break;

                        case CMD_FLASH_WRRD:
                            if (rxlen <= sizeof(urxb.pkt.f.rdcount)
                                || rxlen > 256 + 4 + sizeof(urxb.pkt.f.rdcount))
                                utxb.pkt.head.err = ERR_LEN;
                            rxlen -= sizeof(urxb.pkt.f.rdcount);
                            if (urxb.pkt.f.rdcount > sizeof(utxb.pkt.data))
                                urxb.pkt.f.rdcount = sizeof(utxb.pkt.data);
                            _swire_set_fcs_low();
                            if (rxlen == 1)
                                utxb.pkt.head.count = swire_write_bytes(0x000c, urxb.pkt.f.wrdata, rxlen);
                            else
                                utxb.pkt.head.count = swire_fifo_write(0x000c, urxb.pkt.f.wrdata, rxlen);
                            if (urxb.pkt.f.rdcount) {
                                tmp.ud = (FLD_MASTER_SPI_RD | FLD_MASTER_SPI_SDO) << 8;
                                swire_write_bytes(0x000c, tmp.uc, 2);
                                if (urxb.pkt.f.rdcount == 1)
                                    tmp.ud = swire_read_bytes(0x000c, utxb.pkt.data, urxb.pkt.f.rdcount);
                                else
                                    tmp.ud = swire_fifo_read(0x000c, utxb.pkt.data, urxb.pkt.f.rdcount);
                                utxb.pkt.head.count += tmp.ud;
                                if (tmp.ud != urxb.pkt.f.rdcount)
                                    utxb.pkt.head.err = ERR_READ;
                                utxb.len += tmp.ud;
                            }
                            _swire_set_fcs_hi();
                            break;

                        case CMD_FLASH_RDCRC:
                            if (rxlen == 0
                                || urxb.pkt.rd.len > sizeof(utxb.pkt.data))
                                urxb.pkt.rd.len = sizeof(utxb.pkt.data);
                            rxlen = urxb.pkt.rd.len;
                            if (read_flash(argv, utxb.pkt.data, rxlen) != rxlen)
                                utxb.pkt.head.err = ERR_READ;
                            else
                                utxb.pkt.head.count = crc16_modbus(utxb.pkt.data, rxlen);
                            break;

                        case CMD_SWS_PRINT:
                            if (rxlen > sizeof(urxb.pkt.data))
                                utxb.pkt.head.err = ERR_LEN;
                            else {
                                if (rxlen) {
                                    utxb.pkt.head.count = swire_write_bytes(argv, urxb.pkt.data, rxlen);
                                }
                                sws_pr_txb.len = 0;
                                flg_wait_rx = FLG_CMD_SWS_PRINT;
                            }
                            break;

                        case CMD_WAIT_RESP:
                            if (rxlen && (rxlen > 7 || rxlen < 4))
                                utxb.pkt.head.err = ERR_LEN;
                            else {
                                if (rxlen) {
                                    memcpy(&wrsp_buf.addr, urxb.pkt.data, 3);
                                    wrsp_buf.len = rxlen - 3;
                                    memcpy(wrsp_buf.data, &urxb.pkt.data[3], wrsp_buf.len);
                                } else
                                    wrsp_buf.len = 0;
                                flg_wait_rx = FLG_CMD_WAIT_RESP;
                            }
                            break;

                        case CMD_FUNCS:
                            argv >>= 8;
                            switch (urxb.pkt.head.func) {
                                case CMDF_GET_VERSION:
                                    utxb.pkt.ver.bcd_num = VERSION_BCD;
                                    utxb.pkt.ver.chip_id = EMU_CHIP_ID;
                                    utxb.pkt.ver.pwr = gpio_get(PIN_RESET) != 0;
                                    utxb.pkt.ver.clk_div = swire_clk_div;
                                    utxb.pkt.ver.addr_len = swire_addr_len;
                                    memcpy(utxb.pkt.ver.cmd_cpu_stop, swire_cmd_cpu_stop, sizeof(swire_cmd_cpu_stop));
                                    utxb.pkt.head.count = sizeof(utxb.pkt.ver);
                                    utxb.len += sizeof(utxb.pkt.ver);
                                    break;

                                case CMDF_MCU_REBOOT:
                                    watchdog_reboot(0, 0, 0);
                                    while (1) tight_loop_contents();
                                    break;

                                case CMDF_SWIRE_CFG:
                                    if (rxlen--) {
                                        swire_set_clk_div(urxb.pkt.swcfg.clk_div);
                                        if (rxlen--) {
                                            if (urxb.pkt.swcfg.addr_len == 2)
                                                swire_addr_len = 2;
                                            else
                                                swire_addr_len = 3;
                                            if (rxlen) {
                                                if (rxlen > sizeof(swire_cmd_cpu_stop))
                                                    rxlen = sizeof(swire_cmd_cpu_stop);
                                                memcpy(swire_cmd_cpu_stop, urxb.pkt.swcfg.cmd_cpu_stop, rxlen);
                                            }
                                        }
                                    }
                                    utxb.pkt.swcfg.clk_div = swire_clk_div;
                                    utxb.pkt.swcfg.addr_len = swire_addr_len;
                                    memcpy(utxb.pkt.swcfg.cmd_cpu_stop, swire_cmd_cpu_stop, sizeof(swire_cmd_cpu_stop));
                                    utxb.pkt.head.count = sizeof(utxb.pkt.swcfg);
                                    utxb.len += sizeof(utxb.pkt.swcfg);
                                    break;

                                case CMDF_EXT_POWER:
                                    swire_bus_release();
                                    if (argv) {
                                        set_gpio_reset();
                                        set_gpio_power();
                                    } else {
                                        clr_gpio_reset();
                                        clr_gpio_power();
                                    }
                                    utxb.pkt.data[0] = gpio_get(PIN_RESET) != 0;
                                    utxb.pkt.head.count = 1;
                                    utxb.len++;
                                    break;

                                case CMDF_SWIRE_ACTIVATE:
                                    swire_bus_release();
                                    set_gpio_reset();
                                    set_gpio_power();
                                    swire_write(0xff, FLD_SWIRE_WR | FLD_SWIRE_CMD);
                                    swire_write(0xff, FLD_SWIRE_WR | FLD_SWIRE_CMD);
                                    swire_activate(argv);
                                    utxb.pkt.head.count = swire_read_bytes(0x06bc, utxb.pkt.data, 4);
                                    if (utxb.pkt.head.count != 4)
                                        utxb.pkt.head.err = ERR_READ;
                                    utxb.len += utxb.pkt.head.count;
                                    break;

                                case CMDF_UART_BAUD:
                                    if (argv > 8)
                                        utxb.pkt.head.err = ERR_BAUD;
                                    else
                                        new_uart_baud = (unsigned char)argv;
                                    break;

                                default:
                                    utxb.pkt.head.err = ERR_FUNC;
                                    break;
                            }
                            break;

                        default:
                            utxb.pkt.head.err = ERR_FUNC;
                            utxb.len = 1;
                            break;
                    }
                    if (flg_wait_rx == FLG_CMD_DONE)
                        swire_bus_release();
                }
            }

            cdc_link_block_done();

            if (utxb.len) {
                crc16 = crc16_modbus(utxb.uc, utxb.len);
                utxb.uc[utxb.len++] = (uint8_t)crc16;
                utxb.uc[utxb.len++] = (uint8_t)(crc16 >> 8);
                cdc_link_send(utxb.uc, utxb.len);
                utxb.len = 0;
            }
        }
    }
}
