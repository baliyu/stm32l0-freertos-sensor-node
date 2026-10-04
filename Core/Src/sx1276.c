#include "sx1276.h"
#include "main.h"
#include "cmsis_os.h"

extern SPI_HandleTypeDef hspi1;

/* SX1276 registers used here */
#define REG_FIFO              0x00U
#define REG_OPMODE            0x01U
#define REG_FRF_MSB           0x06U
#define REG_FRF_MID           0x07U
#define REG_FRF_LSB           0x08U
#define REG_PA_CONFIG         0x09U
#define REG_FIFO_ADDR_PTR     0x0DU
#define REG_FIFO_TX_BASE      0x0EU
#define REG_IRQ_FLAGS         0x12U
#define REG_MODEM_CONFIG1     0x1DU
#define REG_MODEM_CONFIG2     0x1EU
#define REG_PREAMBLE_MSB      0x20U
#define REG_PREAMBLE_LSB      0x21U
#define REG_PAYLOAD_LENGTH    0x22U
#define REG_MODEM_CONFIG3     0x26U
#define REG_SYNC_WORD         0x39U

#define OPMODE_SLEEP_LORA     0x80U
#define OPMODE_STDBY_LORA     0x81U
#define OPMODE_TX_LORA        0x83U
#define IRQ_TX_DONE           0x08U

/* Antenna switch on the B-L072Z-LRWAN1 module: PA1 = RX, PC1 = PA_BOOST TX, PC2 = RFO TX */
#define ANT_RX_PORT    GPIOA
#define ANT_RX_PIN     GPIO_PIN_1
#define ANT_BOOST_PORT GPIOC
#define ANT_BOOST_PIN  GPIO_PIN_1
#define ANT_RFO_PORT   GPIOC
#define ANT_RFO_PIN    GPIO_PIN_2

static inline void nss_low(void)  { HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_RESET); }
static inline void nss_high(void) { HAL_GPIO_WritePin(LORA_NSS_GPIO_Port, LORA_NSS_Pin, GPIO_PIN_SET); }

static void antenna_off(void)
{
  HAL_GPIO_WritePin(ANT_RX_PORT,    ANT_RX_PIN,    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ANT_BOOST_PORT, ANT_BOOST_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ANT_RFO_PORT,   ANT_RFO_PIN,   GPIO_PIN_RESET);
}

static void antenna_tx_boost(void)
{
  HAL_GPIO_WritePin(ANT_RX_PORT,    ANT_RX_PIN,    GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ANT_RFO_PORT,   ANT_RFO_PIN,   GPIO_PIN_RESET);
  HAL_GPIO_WritePin(ANT_BOOST_PORT, ANT_BOOST_PIN, GPIO_PIN_SET);
}

static void antenna_pins_init(void)
{
  GPIO_InitTypeDef g = {0};
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  antenna_off();
  g.Mode  = GPIO_MODE_OUTPUT_PP;
  g.Pull  = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  g.Pin = ANT_RX_PIN;                    HAL_GPIO_Init(ANT_RX_PORT, &g);
  g.Pin = ANT_BOOST_PIN | ANT_RFO_PIN;   HAL_GPIO_Init(GPIOC, &g);
}

void sx1276_power_on_reset(void)
{
  antenna_pins_init();

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

int sx1276_lora_init(void)
{
  /* LoRa mode can only be selected from sleep */
  sx1276_write_reg(REG_OPMODE, 0x00U);
  osDelay(2);
  sx1276_write_reg(REG_OPMODE, OPMODE_SLEEP_LORA);
  osDelay(2);

  /* 868.1 MHz: FRF = 868.1e6 * 2^19 / 32e6 = 0xD90666 */
  sx1276_write_reg(REG_FRF_MSB, 0xD9U);
  sx1276_write_reg(REG_FRF_MID, 0x06U);
  sx1276_write_reg(REG_FRF_LSB, 0x66U);

  /* PA_BOOST, +14 dBm (EU868 limit for this sub-band) */
  sx1276_write_reg(REG_PA_CONFIG, 0x80U | (14U - 2U));

  /* BW 125 kHz, CR 4/5, explicit header */
  sx1276_write_reg(REG_MODEM_CONFIG1, 0x72U);
  /* SF7, payload CRC on */
  sx1276_write_reg(REG_MODEM_CONFIG2, 0x74U);
  /* AGC auto on, no low-data-rate optimise (not needed at SF7/125k) */
  sx1276_write_reg(REG_MODEM_CONFIG3, 0x04U);

  /* Preamble 8 symbols, public-style sync word 0x12 (same as the Feather) */
  sx1276_write_reg(REG_PREAMBLE_MSB, 0x00U);
  sx1276_write_reg(REG_PREAMBLE_LSB, 0x08U);
  sx1276_write_reg(REG_SYNC_WORD, 0x12U);

  /* FIFO: use the whole 256 bytes for TX, starting at 0 */
  sx1276_write_reg(REG_FIFO_TX_BASE, 0x00U);

  sx1276_write_reg(REG_OPMODE, OPMODE_STDBY_LORA);
  osDelay(2);

  return (sx1276_read_reg(REG_OPMODE) == OPMODE_STDBY_LORA) ? 1 : 0;
}

int sx1276_send(const uint8_t *data, uint8_t len)
{
  uint8_t buf[65];
  uint8_t i;

  if (len == 0 || len > 64) return 0;

  sx1276_write_reg(REG_OPMODE, OPMODE_STDBY_LORA);
  sx1276_write_reg(REG_IRQ_FLAGS, 0xFFU);              /* clear all flags */
  sx1276_write_reg(REG_FIFO_ADDR_PTR, 0x00U);

  /* Burst write into the FIFO: address byte with write bit, then payload */
  buf[0] = REG_FIFO | 0x80U;
  for (i = 0; i < len; i++) buf[1 + i] = data[i];
  nss_low();
  HAL_SPI_Transmit(&hspi1, buf, (uint16_t)(len + 1U), 100);
  nss_high();

  sx1276_write_reg(REG_PAYLOAD_LENGTH, len);

  antenna_tx_boost();
  sx1276_write_reg(REG_OPMODE, OPMODE_TX_LORA);

  /* Wait for TxDone (about 60 ms for a 20-byte packet), give up after 2 s */
  int ok = 0;
  for (i = 0; i < 200; i++)
  {
    if (sx1276_read_reg(REG_IRQ_FLAGS) & IRQ_TX_DONE) { ok = 1; break; }
    osDelay(10);
  }

  sx1276_write_reg(REG_IRQ_FLAGS, 0xFFU);
  sx1276_write_reg(REG_OPMODE, OPMODE_SLEEP_LORA);     /* low power between packets */
  antenna_off();

  return ok;
}
