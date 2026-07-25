#include "crc16.h"

static uint16_t crc_table[256];

void crc16_init(void) {
    for (unsigned dividend = 0; dividend < 256; dividend++) {
        uint16_t remainder = (uint16_t)dividend;
        for (unsigned bit = 0; bit < 8; bit++) {
            remainder = (remainder & 1u) ? (uint16_t)((remainder >> 1) ^ 0xA001u)
                                         : (uint16_t)(remainder >> 1);
        }
        crc_table[dividend] = remainder;
    }
}

uint16_t crc16_modbus(const uint8_t *message, size_t nbytes) {
    uint16_t remainder = 0xFFFFu;
    while (nbytes--) {
        remainder = (uint16_t)((remainder >> 8) ^ crc_table[(remainder ^ *message++) & 0xFFu]);
    }
    return remainder;
}
