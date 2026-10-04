#include "sx1276.h"
#include "main.h"
#include "cmsis_os.h"

extern SPI_HandleTypeDef hspi1;

static inline void nss_low(void)  { HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET); }
static inline void nss_high(void) { HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET); }

void sx1276_power_on_reset(void)
{
  /* The radio's 32 MHz clock (TCXO) is powered from a GPIO */
  HAL_GPIO_WritePin(LORA_TCXO_GPIO_Port, LORA_TCXO_Pin, GPIO_PIN_SET);
  osDelay(10);                                   /* TCXO needs a few ms to settle */

  nss_high();

  /* Reset: pull low for at least 100 us, then release and wait 5 ms */
  HAL_GPIO_WritePin(LORA_RESET_GPIO_Port, LORA_RESET_Pin, GPIO_PIN_RESET);
  osDelay(2);
  HAL_GPIO_WritePin(LORA_RESET_GPIO_Port, LORA_RESET_Pin, GPIO_PIN_SET);
  osDelay(10);
}

uint8_t sx1276_read_reg(uint8_t addr)
{
  uint8_t tx[2] = { (uint8_t)(addr & 0x7FU), 0x00U };   /* bit 7 = 0: read */
  uint8_t rx[2] = { 0, 0 };

  nss_low();
  HAL_SPI_TransmitReceive(&hspi1, tx, rx, 2, 100);
  nss_high();

  return rx[1];
}

void sx1276_write_reg(uint8_t addr, uint8_t value)
{
  uint8_t tx[2] = { (uint8_t)(addr | 0x80U), value };   /* bit 7 = 1: write */

  nss_low();
  HAL_SPI_Transmit(&hspi1, tx, 2, 100);
  nss_high();
}
