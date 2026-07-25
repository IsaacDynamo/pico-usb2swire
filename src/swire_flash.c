#include "proto.h"
#include "swire.h"
#include "swire_flash.h"

void _swire_set_fcs_low(void) {
    unsigned char tmp = 0;
    swire_write_bytes(0x000d, &tmp, 1);
}

void _swire_set_fcs_hi(void) {
    unsigned char tmp = FLD_MASTER_SPI_CS;
    swire_write_bytes(0x000d, &tmp, 1);
}

void _swire_fcmd_faddr(unsigned char cmd, unsigned int addr) {
    union {
        unsigned int  ud;
        unsigned char uc[4];
    } tmp;
    _swire_set_fcs_low();
    tmp.ud = addr;
    tmp.uc[3] = cmd;
    swire_write_bytes(0x000c, &tmp.uc[3], 1);
    swire_write_bytes(0x000c, &tmp.uc[2], 1);
    swire_write_bytes(0x000c, &tmp.uc[1], 1);
    swire_write_bytes(0x000c, &tmp.uc[0], 1);
}

void swire_fcmd(unsigned char fcmd) {
    unsigned char tmp = fcmd;
    _swire_set_fcs_low();
    swire_write_bytes(0x000c, &tmp, 1);
    _swire_set_fcs_hi();
}

unsigned int read_flash(unsigned int faddr, unsigned char *pbuf, unsigned int cnt) {
    union {
        unsigned int  ud;
        unsigned char uc[4];
    } rxlen;

    _swire_fcmd_faddr(FLASH_READ_CMD, faddr);
    rxlen.ud = (FLD_MASTER_SPI_RD | FLD_MASTER_SPI_SDO) << 8;
    swire_write_bytes(0x000c, rxlen.uc, 2);

    if (cnt == 1)
        rxlen.ud = swire_read_bytes(0x000c, pbuf, cnt);
    else
        rxlen.ud = swire_fifo_read(0x000c, pbuf, cnt);

    _swire_set_fcs_hi();
    return rxlen.ud;
}
