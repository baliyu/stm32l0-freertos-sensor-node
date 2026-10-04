/* fcnt_eeprom.c - fcnt_store backend using the STM32L0's built-in data EEPROM.
 *
 * Uses the first 16 bytes of data EEPROM (DATA_EEPROM_BASE = 0x08080000 on
 * STM32L0): two slots of (value, ~value). The L0 data EEPROM is separate from
 * program flash, is word-writable without a manual erase, and is rated for far
 * more write cycles than flash (check the datasheet's endurance figure).
 */
#include "main.h"
#include "fcnt_store.h"

#define FCNT_EE_ADDR(i)  (DATA_EEPROM_BASE + 4U * (i))

static uint32_t ee_read(uint32_t i)
{
  return *(volatile const uint32_t *)FCNT_EE_ADDR(i);   /* EEPROM is memory-mapped: read directly */
}

static int ee_write(uint32_t i, uint32_t v)
{
  HAL_StatusTypeDef st;

  if (HAL_FLASHEx_DATAEEPROM_Unlock() != HAL_OK) return -1;
  st = HAL_FLASHEx_DATAEEPROM_Program(FLASH_TYPEPROGRAMDATA_WORD, FCNT_EE_ADDR(i), v);
  HAL_FLASHEx_DATAEEPROM_Lock();                        /* re-lock against stray writes */

  return (st == HAL_OK) ? 0 : -1;
}

const fcnt_backend fcnt_eeprom_backend = { ee_read, ee_write };
