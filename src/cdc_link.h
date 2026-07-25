#ifndef CDC_LINK_H_
#define CDC_LINK_H_

#include <stdint.h>
#include <stdbool.h>

void cdc_link_init(void);
void cdc_link_task(void);
bool cdc_link_block_ready(void);
void cdc_link_block_done(void);
bool cdc_link_rx_started(void);
void cdc_link_send(const uint8_t *data, uint32_t len);
void cdc_link_reset(void);
void cdc_link_on_host_reset(void);

#endif /* CDC_LINK_H_ */
