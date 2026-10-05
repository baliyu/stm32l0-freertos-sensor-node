/* flash_l0.c - erase 128-byte pages and program words in STM32L0 program
 * memory, register level (RM0367: unlock PECR with PEKEY1/2, then program
 * memory with PRGKEY1/2; page erase = set ERASE+PROG and write 0 to the page).
 *
 * Writes are refused outside slots A and B, so a bug here can never erase
 * the bootloader itself. */
#include "stm32l0xx.h"
#include "flash_l0.h"

#define PEKEY1   0x89ABCDEFUL
#define PEKEY2   0x02030405UL
#define PRGKEY1  0x8C9DAEBFUL
#define PRGKEY2  0x13141516UL
#define SR_ERRORS (FLASH_SR_WRPERR | FLASH_SR_PGAERR | FLASH_SR_SIZERR | FLASH_SR_OPTVERR | \
                   FLASH_SR_RDERR | FLASH_SR_NOTZEROERR | FLASH_SR_FWWERR)

void (*flash_l0_progress)(uint32_t done, uint32_t total) = 0;

static int in_slots(uint32_t addr)
{
  return addr >= SLOT_A_BASE && addr < SLOT_B_BASE + SLOT_SIZE;
}

static void unlock(void)
{
  while (FLASH->SR & FLASH_SR_BSY) { }
  FLASH->SR = SR_ERRORS | FLASH_SR_EOP;
  if (FLASH->PECR & FLASH_PECR_PELOCK)  { FLASH->PEKEYR = PEKEY1;   FLASH->PEKEYR = PEKEY2; }
  if (FLASH->PECR & FLASH_PECR_PRGLOCK) { FLASH->PRGKEYR = PRGKEY1; FLASH->PRGKEYR = PRGKEY2; }
}

static int finish(void)
{
  int ok;
  while (FLASH->SR & FLASH_SR_BSY) { }
  ok = (FLASH->SR & SR_ERRORS) == 0U;
  FLASH->SR = SR_ERRORS | FLASH_SR_EOP;
  FLASH->PECR |= FLASH_PECR_PELOCK;                    /* relocks program memory too */
  return ok ? 0 : -1;
}

static const uint8_t *ptr(uint32_t addr) { return (const uint8_t *)addr; }

static int erase_page(uint32_t addr)
{
  int r;
  if (!in_slots(addr) || (addr % FLASH_PAGE_SIZE) != 0U) return -1;
  unlock();
  FLASH->PECR |= FLASH_PECR_ERASE | FLASH_PECR_PROG;
  *(volatile uint32_t *)addr = 0;                      /* any write in the page starts the erase */
  r = finish();
  FLASH->PECR &= ~(FLASH_PECR_ERASE | FLASH_PECR_PROG);
  return r;
}

static int write_word(uint32_t addr, uint32_t value)
{
  if (!in_slots(addr) || (addr & 3U) != 0U) return -1;
  unlock();
  *(volatile uint32_t *)addr = value;
  return finish();
}

static void progress(uint32_t done, uint32_t total)
{
  if (flash_l0_progress) flash_l0_progress(done, total);
}

const flash_ops flash_l0 = { ptr, erase_page, write_word, progress };
