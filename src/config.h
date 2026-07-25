#ifndef CONFIG_H_
#define CONFIG_H_

// pins
#define PIN_SWS     0
#define PIN_RESET   3
#define PIN_POWER   4 // optional: don't bother usually

// 0x5562 is a 24 MHz TLSR825x so swdiv 5 gives exactly 24/5/5 = 0.9600 Mbit/s
#define EMU_CHIP_ID         0x5562u
#define EMU_CORE_HZ         24000000u
#define SWIRE_CLK_DIV_DEFAULT 5u
#define SWIRE_SYS_CLK_KHZ   96000u

#define SWIRE_UNITS_PER_BIT 5u
#define SWIRE_PIO_TICKS_PER_UNIT 4u

#define VERSION_BCD         0x0008

#define DATA_BUFF_SIZE      1024

// head(4) + payload(1024) + crc(2)
#define IO_BUFF_SIZE        (DATA_BUFF_SIZE + 16)

// START + 3 address + RW_ID + 1024 data + END.
#define SWIRE_MAX_FRAME_BYTES (1u + 3u + 1u + DATA_BUFF_SIZE + 1u)

// head(4) + 1024 + crc(2).
#define USB_MAX_BLK_SIZE      (4u + DATA_BUFF_SIZE + 2u)

#define USB_RX_IDLE_TIMEOUT_US 1750u

#endif /* CONFIG_H_ */
