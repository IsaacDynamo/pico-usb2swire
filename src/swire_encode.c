#include "swire_encode.h"

#define SLOT_ZERO 0x1Eu
#define SLOT_ONE  0x10u

#define ENC5(x)   (x)
#define IDLE_BITS 0xFFFFFFFFu

static uint32_t nib_lut[16];

void sw_enc_init(void) {
    for (unsigned v = 0; v < 16; v++) {
        uint32_t p = 0;
        for (unsigned i = 0; i < 4; i++) {
            uint32_t slot = ((v >> (3u - i)) & 1u) ? ENC5(SLOT_ONE) : ENC5(SLOT_ZERO);
            p |= slot << (5u * i);
        }
        nib_lut[v] = p;
    }
}

static void emit(sw_enc_t *e, uint32_t val, uint32_t n) {
    uint32_t nbits = e->nbits;
    uint32_t total = nbits + n;

    e->acc |= val << nbits;
    e->nunits += n;

    if (total >= 32u) {
        if (e->nwords < e->cap) e->buf[e->nwords] = e->acc;
        e->nwords++;
        e->acc = nbits ? (val >> (32u - nbits)) : 0u;
        e->nbits = total - 32u;
    } else {
        e->nbits = total;
    }
}

void sw_enc_begin(sw_enc_t *e, uint32_t *buf, uint32_t cap_words) {
    e->buf = buf;
    e->cap = cap_words;
    e->acc = 0;
    e->nbits = 0;
    e->nwords = 0;
    e->nunits = 0;
}

void sw_enc_byte(sw_enc_t *e, uint8_t data, bool cmd) {
    emit(e, cmd ? ENC5(SLOT_ONE) : ENC5(SLOT_ZERO), 5);
    emit(e, nib_lut[data >> 4], 20);
    emit(e, nib_lut[data & 0x0Fu], 20);
    emit(e, ENC5(SLOT_ZERO), 5);
}

void sw_enc_idle(sw_enc_t *e, uint32_t units) {
    while (units) {
        uint32_t n = units > 16u ? 16u : units;
        emit(e, IDLE_BITS & ((1u << n) - 1u), n);
        units -= n;
    }
}

uint32_t sw_enc_end(sw_enc_t *e) {
    if (e->nbits) sw_enc_idle(e, 32u - e->nbits);
    if (e->nwords < e->cap) e->buf[e->nwords] = IDLE_BITS;
    e->nwords++;
    e->nunits += 32u;
    return e->nwords;
}
