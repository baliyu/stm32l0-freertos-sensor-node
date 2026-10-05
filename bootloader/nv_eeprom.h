/* nv_eeprom.h - data EEPROM backend for the bootloader (register level, no HAL).
 *
 * Data EEPROM map (STM32L072, 6 KB at 0x08080000):
 *   0x08080000  16 B  application: LoRa frame counter (Core/Src/fcnt_eeprom.c)
 *   0x08080100  16 B  bootloader:  minimum allowed firmware version (this file)
 */
#ifndef NV_EEPROM_H
#define NV_EEPROM_H

#include "rollback.h"

#define ROLLBACK_EEPROM_ADDR  0x08080100UL

extern const nv_backend rollback_eeprom;

#endif
