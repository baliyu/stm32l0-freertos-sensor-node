#ifndef SX1276_H
#define SX1276_H

#include <stdint.h>

#define SX1276_REG_VERSION   0x42U
#define SX1276_VERSION_OK    0x12U   /* value every SX1276 reports */

/* Power the TCXO, pulse the radio reset line, wait for it to settle.
   Call from a task (it uses osDelay). */
void    sx1276_power_on_reset(void);

uint8_t sx1276_read_reg(uint8_t addr);
void    sx1276_write_reg(uint8_t addr, uint8_t value);

#endif /* SX1276_H */
