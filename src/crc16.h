#ifndef CRC16_H_
#define CRC16_H_

#include <stdint.h>
#include <stddef.h>

void     crc16_init(void);
uint16_t crc16_modbus(const uint8_t *message, size_t nbytes);

#endif /* CRC16_H_ */
