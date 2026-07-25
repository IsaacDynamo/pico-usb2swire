#ifndef PROTO_H_
#define PROTO_H_

#include <stdint.h>
#include "config.h"

typedef enum {
    CMD_FUNCS = 0,          //0
    CMD_FLASH_READ,         //1
    CMD_FLASH_WRITE,        //2
    CMD_FLASH_SECT_ERASE,   //3
    CMD_FLASH_ALL_ERASE,    //4
    CMD_FLASH_GET_JEDEC_ID, //5
    CMD_FLASH_GET_STATUS,   //6
    CMD_SWIRE_READ,         //7
    CMD_SWIRE_WRITE,        //8
    CMD_SWIRE_AREAD,        //9
    CMD_SWIRE_AWRITE,       //a
    CMD_SWIRE_FIFO_READ,    //b
    CMD_SWIRE_FIFO_FWRITE,  //c
    CMD_FLASH_WRRD,         //d
    CMD_FLASH_RDCRC,        //e
    CMD_SWS_PRINT,          //f
    CMD_WAIT_RESP           //10
} E_CMD_t;

typedef enum {
    CMDF_GET_VERSION = 0,   //0
    CMDF_MCU_REBOOT,        //1
    CMDF_SWIRE_CFG,         //2
    CMDF_EXT_POWER,         //3
    CMDF_SWIRE_ACTIVATE,    //4
    CMDF_UART_BAUD,         //5
} E_CMDF_t;

typedef enum {
    ERR_NONE = 0,   //0 Errors None
    ERR_FUNC,       //1 Function number error
    ERR_LEN,        //2 Data length error
    ERR_READ,       //3 Swire read timeout
    ERR_BUSY,       //4 Timeout flag while reading analog register
    ERR_CRC,        //5 Error CRC
    ERR_BAUD        //6 Invalid baud rate number
} E_ERR_t;

typedef union __attribute__((packed)) _blk_head_t {
    struct __attribute__((packed)) {
        uint8_t cmd; // function number (input/output stream)
        union __attribute__((packed)) {
            struct __attribute__((packed)) {
                union __attribute__((packed)) {
                    uint8_t func; // subfunction number (input stream CMD_FUNCS)
                    uint8_t err; // error number (output stream)
                };
                uint16_t count; // number of bytes worked (output stream)
            };
            uint8_t argv[3]; // argument value (input stream)
        };
    };
    struct __attribute__((packed)) {
        uint32_t cmd  : 8;  // function number (input/output stream)
        uint32_t argv : 24; // argument value (input stream)
    } b;
    uint32_t ud;
} blk_head_t;

typedef struct __attribute__((packed)) _cmd_read_data_t {
    uint16_t len;
} cmd_read_data_t;

typedef struct __attribute__((packed)) _cmd_swire_cfg_t {
    uint8_t clk_div;
    uint8_t addr_len;
    uint8_t cmd_cpu_stop[6];
} cmd_swire_cfg_t;

typedef struct __attribute__((packed)) _cmd_flash_wrrd_t {
    uint16_t rdcount;
    uint8_t  wrdata[256];
} cmd_flash_wrrd_t;

typedef struct __attribute__((packed)) _cmd_version_t {
    uint16_t bcd_num;
    uint16_t chip_id;
    uint8_t  pwr;
    uint8_t  clk_div;
    uint8_t  addr_len;
    uint8_t  cmd_cpu_stop[6];
} cmd_version_t;

typedef struct __attribute__((packed)) _blk_rx_pkt_t {
    blk_head_t head;
    union __attribute__((packed)) {
        cmd_read_data_t   rd;
        cmd_swire_cfg_t   swcfg;
        cmd_version_t     ver;
        cmd_flash_wrrd_t  f;
        uint8_t  uc[DATA_BUFF_SIZE];
        uint16_t uw[DATA_BUFF_SIZE / 2];
        uint32_t ud[DATA_BUFF_SIZE / 4];
        uint8_t  data[DATA_BUFF_SIZE];
    };
} blk_rx_pkt_t;

typedef struct _io_buf_t {
    volatile uint32_t len;
    union {
        blk_rx_pkt_t pkt;
        uint8_t  uc[IO_BUFF_SIZE];
        uint16_t uw[IO_BUFF_SIZE / 2];
        uint32_t ud[IO_BUFF_SIZE / 4];
    };
} io_buf_t;

typedef struct _sws_pintf_buf_t {
    uint8_t id;
    uint8_t len;
    uint8_t data[254];
} sws_pintf_buf_t;

#define FLD_MASTER_SPI_CS   (1u << 0)
#define FLD_MASTER_SPI_SDO  (1u << 1)
#define FLD_MASTER_SPI_CONT (1u << 2)
#define FLD_MASTER_SPI_RD   (1u << 3)
#define FLD_MASTER_SPI_BUSY (1u << 4)

#define FLD_ANA_BUSY        (1u << 0)
#define FLD_ANA_START       (1u << 6)

#define FLASH_WRITE_CMD             0x02
#define FLASH_READ_CMD              0x03
#define FLASH_WRITE_ENABLE_CMD      0x06
#define FLASH_WRITE_DISABLE_CMD     0x04
#define FLASH_READ_STATUS_CMD       0x05
#define FLASH_SECT_ERASE_CMD        0x20
#define FLASH_32KBLK_ERASE_CMD      0x52
#define FLASH_CHIP_ERASE_CMD        0x60
#define FLASH_GET_JEDEC_ID          0x9F
#define FLASH_PAGE_ERASE_CMD        0x81
#define FLASH_BLK_ERASE_CMD         0xD8

extern io_buf_t urxb;
extern io_buf_t utxb;

#endif /* PROTO_H_ */
