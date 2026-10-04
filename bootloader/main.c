/* bootloader/main.c - secure boot, stage 2: verify the image in slot A
 * (header + SHA-256), then hand over to it. Plain register access, no HAL.
 *
 * Next stages: ECDSA signature over the hash (3), anti-rollback (4),
 * updates through slot B (5), write/read protection (6).
 */
#include "stm32l0xx.h"
#include "image.h"
#include <stdint.h>

/* ---------- Clock: HSI16 so hashing ~50 KB takes a fraction of a second ---------- */
static void clock_hsi16(void)
{
  FLASH->ACR |= FLASH_ACR_LATENCY;               /* 1 wait state: needed above 8 MHz at reset voltage */
  RCC->CR |= RCC_CR_HSION;
  while (!(RCC->CR & RCC_CR_HSIRDY)) { }
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI) { }
}

static void clock_restore(void)                  /* back to the reset clock (MSI) for the app */
{
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_MSI;
  while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_MSI) { }
  RCC->CR &= ~RCC_CR_HSION;
}

/* ---------- 1 ms tick, only to report how long the check took ---------- */
static volatile uint32_t g_ms;
void SysTick_Handler(void) { g_ms++; }

static void tick_start(void)
{
  SysTick->LOAD = 16000U - 1U;                   /* 16 MHz / 16000 = 1 kHz */
  SysTick->VAL  = 0;
  SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
}

static void tick_stop(void)
{
  SysTick->CTRL = 0;
  SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;            /* drop any tick still pending */
}

/* ---------- Minimal UART (USART2 on PA2 = ST-LINK virtual COM port) ---------- */
static void uart_init(void)
{
  RCC->IOPENR  |= RCC_IOPENR_GPIOAEN;
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;
  GPIOA->MODER  = (GPIOA->MODER & ~(3UL << (2 * 2))) | (2UL << (2 * 2));   /* PA2: alternate function */
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFUL << (2 * 4))) | (4UL << (2 * 4)); /* AF4 = USART2_TX */
  USART2->BRR = 139U;                            /* 16 MHz / 115200 = 138.9 */
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

static void uart_puts(const char *s)
{
  while (*s)
  {
    while (!(USART2->ISR & USART_ISR_TXE)) { }
    USART2->TDR = (uint8_t)*s++;
  }
  while (!(USART2->ISR & USART_ISR_TC)) { }
}

static void uart_puthex32(uint32_t v)
{
  static const char hex[] = "0123456789ABCDEF";
  char b[11] = "0x";
  for (int i = 0; i < 8; i++) b[2 + i] = hex[(v >> (28 - 4 * i)) & 0xFU];
  b[10] = '\0';
  uart_puts(b);
}

static void uart_putbytes(const uint8_t *p, int n)
{
  static const char hex[] = "0123456789abcdef";
  char b[3] = { 0, 0, 0 };
  for (int i = 0; i < n; i++) { b[0] = hex[p[i] >> 4]; b[1] = hex[p[i] & 0xFU]; uart_puts(b); }
}

static void uart_putdec(uint32_t v)
{
  char b[11];
  int i = 10;
  b[i] = '\0';
  do { b[--i] = (char)('0' + (v % 10U)); v /= 10U; } while (v && i > 0);
  uart_puts(&b[i]);
}

/* ---------- Hand-over ---------- */
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

static void jump_to_app(uint32_t sp, uint32_t reset)
{
  SCB->VTOR = APP_BASE;                          /* the app's interrupts use the app's vector table */
  __DSB();
  __ISB();
  __asm volatile ("msr msp, %0 \n"               /* app's stack, then its reset handler */
                  "bx  %1      \n"
                  : : "r" (sp), "r" (reset) : "memory");
}

static void halt(void)
{
  uart_puts("[BOOT] refusing to start the application - halted\r\n");
  for (;;) { }
}

int main(void)
{
  const uint8_t *slot = (const uint8_t *)SLOT_A_BASE;
  const img_header *h = (const img_header *)SLOT_A_BASE;
  uint8_t digest[32];
  img_result r;
  uint32_t t0, dt;

  clock_hsi16();
  uart_init();
  tick_start();

  uart_puts("\r\n[BOOT] stage 2 bootloader: header + SHA-256 check\r\n");

  t0 = g_ms;
  r = image_verify(slot, SLOT_A_BASE, digest);
  dt = g_ms - t0;

  if (r == IMG_ERR_MAGIC)
  {
    uart_puts("[BOOT] magic ");
    uart_puthex32(h->magic);
    uart_puts(": ");
    uart_puts(image_result_str(r));
    uart_puts("\r\n");
    halt();
  }

  uart_puts("[BOOT] image v");
  uart_putdec(h->fw_version >> 24);
  uart_puts(".");
  uart_putdec((h->fw_version >> 16) & 0xFFU);
  uart_puts(".");
  uart_putdec(h->fw_version & 0xFFFFU);
  uart_puts(", ");
  uart_putdec(h->img_size);
  uart_puts(" bytes\r\n");

  if (r != IMG_OK)
  {
    uart_puts("[BOOT] ");
    uart_puts(image_result_str(r));
    uart_puts("\r\n");
    if (r == IMG_ERR_HASH)
    {
      uart_puts("[BOOT]   expected ");
      uart_putbytes(h->sha256, 8);
      uart_puts("...\r\n[BOOT]   computed ");
      uart_putbytes(digest, 8);
      uart_puts("...\r\n");
    }
    halt();
  }

  uart_puts("[BOOT] SHA-256 OK (");
  uart_putbytes(digest, 8);
  uart_puts("...) in ");
  uart_putdec(dt);
  uart_puts(" ms\r\n[BOOT] jumping to application\r\n");

  {
    const uint8_t *app = slot + IMG_HDR_SIZE;
    uint32_t sp    = (uint32_t)app[0] | ((uint32_t)app[1] << 8) | ((uint32_t)app[2] << 16) | ((uint32_t)app[3] << 24);
    uint32_t reset = (uint32_t)app[4] | ((uint32_t)app[5] << 8) | ((uint32_t)app[6] << 16) | ((uint32_t)app[7] << 24);

    tick_stop();
    peripherals_reset();
    clock_restore();
    jump_to_app(sp, reset);
  }

  for (;;) { }
}
