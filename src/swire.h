#ifndef SWIRE_H_
#define SWIRE_H_

#include <stdint.h>
#include <stdbool.h>

#define FLD_SWIRE_WR    (1u << 0)
#define FLD_SWIRE_RD    (1u << 1)
#define FLD_SWIRE_CMD   (1u << 2)

unsigned int swire_read(unsigned char *data, unsigned int len);
void         swire_write(unsigned char data, unsigned char ctrl);

unsigned int swire_read_bytes(unsigned int addr, unsigned char *pdata, unsigned int len);
unsigned int swire_write_bytes(unsigned int addr, unsigned char *pdata, unsigned int len);
unsigned int swire_fifo_read(unsigned int addr, unsigned char *pdata, unsigned int len);
unsigned int swire_fifo_write(unsigned int addr, unsigned char *pdata, unsigned int len);

extern unsigned char swire_addr_len;
extern unsigned char swire_cmd_cpu_stop[6];

extern unsigned char swire_clk_div;

void swire_init(void);
void swire_set_clk_div(unsigned char div);
void swire_bus_release(void);
void swire_activate(unsigned int count);
uint32_t swire_unit_ns(void);

#endif /* SWIRE_H_ */
