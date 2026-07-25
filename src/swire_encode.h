#ifndef SWIRE_ENCODE_H_
#define SWIRE_ENCODE_H_

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define SWIRE_UNITS_PER_BYTE 50u

#define SWIRE_ENC_WORDS (((SWIRE_MAX_FRAME_BYTES * SWIRE_UNITS_PER_BYTE) + 31u) / 32u + 1u)

typedef struct {
    uint32_t *buf;
    uint32_t  cap;
    uint32_t  acc;
    uint32_t  nbits;
    uint32_t  nwords;
    uint32_t  nunits;
} sw_enc_t;

void sw_enc_init(void);

void sw_enc_begin(sw_enc_t *e, uint32_t *buf, uint32_t cap_words);
void sw_enc_byte(sw_enc_t *e, uint8_t data, bool cmd);
void sw_enc_idle(sw_enc_t *e, uint32_t units);

uint32_t sw_enc_end(sw_enc_t *e);

#define SW_ENC_TAIL_UNITS 32u

#endif /* SWIRE_ENCODE_H_ */
