/* nv_eeprom.c - read/write words in the STM32L0 data EEPROM without the HAL.
 * The EEPROM is memory-mapped for reading. Writing needs it unlocked with two
 * key values (RM0367, "Unlocking the data EEPROM"); a word write erases and
 * programs in one go when FLASH_PECR.FIX is clear (the reset state). */
#include "stm32l0xx.h"
#include "nv_eeprom.h"

#define PEKEY1  0x89ABCDEFUL
#define PEKEY2  0x02030405UL
#define SR_ERRORS (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_OPTVERR | \
                   FLASH_SR_RDERR | FLASH_SR_NOTZEROERR | FLASH_SR_FWWERR)

static uint32_t ee_read(uint32_t i)
{
  return *(volatile const uint32_t *)(ROLLBACK_EEPROM_ADDR + 4U * i);
}

static int ee_write(uint32_t i, uint32_t v)
{
  int ok;

  while (FLASH->SR & FLASH_SR_BSY) { }
  FLASH->SR = SR_ERRORS;                                  /* clear old error flags (write 1) */

  if (FLASH->PECR & FLASH_PECR_PELOCK)                    /* unlock */
  {
    FLASH->PEKEYR = PEKEY1;
    FLASH->PEKEYR = PEKEY2;
  }

  *(volatile uint32_t *)(ROLLBACK_EEPROM_ADDR + 4U * i) = v;
  while (FLASH->SR & FLASH_SR_BSY) { }

  ok = (FLASH->SR & SR_ERRORS) == 0U;
  FLASH->SR = SR_ERRORS | FLASH_SR_EOP;
  FLASH->PECR |= FLASH_PECR_PELOCK;                       /* lock again */

  return ok ? 0 : -1;
}

const nv_backend rollback_eeprom = { ee_read, ee_write };
