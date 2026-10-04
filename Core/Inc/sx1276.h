#ifndef SX1276_H
#define SX1276_H

#include <stdint.h>

#define SX1276_REG_VERSION   0x42U
#define SX1276_VERSION_OK    0x12U   /* value every SX1276 reports */

/* Power the TCXO, set up the antenna-switch pins, pulse reset.
   Call from a task (it uses osDelay). */
void    sx1276_power_on_reset(void);

/* LoRa mode, 868.1 MHz, SF7, BW125, CR4/5, sync word 0x12, +14 dBm.
   Returns 1 if the radio is in standby afterwards, 0 on failure. */
int     sx1276_lora_init(void);

/* Send up to 64 bytes and wait for TX done. Returns 1 on success. */
int     sx1276_send(const uint8_t *data, uint8_t len);

uint8_t sx1276_read_reg(uint8_t addr);
void    sx1276_write_reg(uint8_t addr, uint8_t value);

#endif /* SX1276_H */
