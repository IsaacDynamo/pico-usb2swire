#ifndef SWIRE_FLASH_H_
#define SWIRE_FLASH_H_

void _swire_set_fcs_low(void);
void _swire_set_fcs_hi(void);
void _swire_fcmd_faddr(unsigned char cmd, unsigned int addr);
void swire_fcmd(unsigned char fcmd);
unsigned int read_flash(unsigned int faddr, unsigned char *pbuf, unsigned int cnt);

#endif /* SWIRE_FLASH_H_ */
