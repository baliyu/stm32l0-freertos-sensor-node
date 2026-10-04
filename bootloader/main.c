/* bootloader/main.c - stage 1 of secure boot: check that slot A holds something
 * that looks like an application, then hand over to it. No HAL: plain register
 * access keeps the bootloader small and makes every step visible.
 *
 * Later stages add: image header check, SHA-256, ECDSA signature, anti-rollback.
 */
#include "stm32l0xx.h"
#include <stdint.h>

#define SLOT_A_BASE    0x08006000UL
#define HEADER_SIZE    0x200UL                       /* 512-byte image header (used from stage 2) */
#define APP_BASE       (SLOT_A_BASE + HEADER_SIZE)   /* app vector table: 0x08006200 */
#define SLOT_A_END     0x08018000UL
#define RAM_START      0x20000000UL
#define RAM_END        (0x20000000UL + 20UL * 1024UL)

/* ---------- Minimal UART (USART2 on PA2 = ST-LINK virtual COM port) ---------- */
static void uart_init(void)
{
  RCC->IOPENR  |= RCC_IOPENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  GPIOA->MODER  = (GPIOA->MODER & ~(3UL << (2 * 2))) | (2UL << (2 * 2));   /* PA2: alternate function */
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFUL << (2 * 4))) | (4UL << (2 * 4)); /* AF4 = USART2_TX */

  /* After reset the core runs from MSI at 2.097 MHz: 2097152 / 115200 = 18.2 -> 18 (1% error) */
  USART2->BRR = 18U;
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart_puts(const char *s)
{
  while (*s)
  {
    while (!(USART2->ISR & USART_ISR_TXE)) { }
    USART2->TDR = (uint8_t)*s++;
  }
  while (!(USART2->ISR & USART_ISR_TC)) { }   /* last byte fully sent */
}

static void uart_puthex(uint32_t v)
{
  static const char hex[] = "0123456789ABCDEF";
  char buf[11] = "0x";
  for (int i = 0; i < 8; i++) buf[2 + i] = hex[(v >> (28 - 4 * i)) & 0xFU];
  buf[10] = '\0';
  uart_puts(buf);
}

/* Put everything the bootloader touched back to its reset state */
static void peripherals_reset(void)
{
  USART2->CR1 = 0;
  RCC->APB1RSTR |=  RCC_APB1RSTR_USART2RST;
  RCC->APB1RSTR &= ~RCC_APB1RSTR_USART2RST;
  RCC->APB1ENR  &= ~RCC_APB1ENR_USART2EN;
  RCC->IOPRSTR  |=  RCC_IOPRSTR_GPIOARST;
  RCC->IOPRSTR  &= ~RCC_IOPRSTR_GPIOARST;
  RCC->IOPENR   &= ~RCC_IOPENR_GPIOAEN;
}

/* A plausible Cortex-M image: initial stack pointer inside RAM, reset handler
 * inside slot A and a Thumb address (lowest bit set). Erased L0 flash reads 0,
 * so an empty slot fails this check. This is NOT security yet, just sanity. */
static int app_looks_valid(uint32_t sp, uint32_t reset)
{
  if (sp < RAM_START || sp > RAM_END || (sp & 3U) != 0) return 0;
  if ((reset & 1U) == 0) return 0;
  if (reset < APP_BASE || reset >= SLOT_A_END) return 0;
  return 1;
}

static void jump_to_app(uint32_t sp, uint32_t reset)
{
  SysTick->CTRL = 0;                 /* nothing should be ticking when the app starts */
  SCB->VTOR = APP_BASE;              /* the app's interrupts use the app's vector table */
  __DSB();
  __ISB();

  /* Load the app's stack pointer and branch to its reset handler in one go,
   * in assembly, so the compiler cannot touch the old stack in between. */
  __asm volatile ("msr msp, %0 \n"
                  "bx  %1      \n"
                  : : "r" (sp), "r" (reset) : "memory");
}

int main(void)
{
  uint32_t sp    = *(volatile const uint32_t *)(APP_BASE);
  uint32_t reset = *(volatile const uint32_t *)(APP_BASE + 4U);

  uart_init();
  uart_puts("\r\n[BOOT] stage 1 bootloader\r\n[BOOT] slot A vectors at ");
  uart_puthex(APP_BASE);
  uart_puts(": SP=");
  uart_puthex(sp);
  uart_puts(" reset=");
  uart_puthex(reset);
  uart_puts("\r\n");

  if (!app_looks_valid(sp, reset))
  {
    uart_puts("[BOOT] no valid application in slot A - halted\r\n");
    for (;;) { }
  }

  uart_puts("[BOOT] jumping to application\r\n");
  peripherals_reset();
  jump_to_app(sp, reset);

  for (;;) { }                       /* not reached */
}
