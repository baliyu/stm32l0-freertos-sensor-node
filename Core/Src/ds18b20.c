#include "ds18b20.h"
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

extern TIM_HandleTypeDef htim2;   /* defined in main.c, 1 tick = 1 us */

void ds18b20_init(void)
{
  HAL_TIM_Base_Start(&htim2);
}

static void delay_us(uint16_t us)
{
  __HAL_TIM_SET_COUNTER(&htim2, 0);
  while (__HAL_TIM_GET_COUNTER(&htim2) < us) { }
}

/* 1-Wire low level (open-drain pin, external pull-up) */
static inline void ow_low(void)     { HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_RESET); }
static inline void ow_release(void) { HAL_GPIO_WritePin(DS18B20_GPIO_Port, DS18B20_Pin, GPIO_PIN_SET); }
static inline uint8_t ow_read_pin(void) { return HAL_GPIO_ReadPin(DS18B20_GPIO_Port, DS18B20_Pin); }

/* Reset pulse; returns 1 if a sensor answered with a presence pulse */
static uint8_t ow_reset(void)
{
  uint8_t presence;
  taskENTER_CRITICAL();
  ow_low();
  delay_us(480);
  ow_release();
  delay_us(70);
  presence = (ow_read_pin() == GPIO_PIN_RESET);
  taskEXIT_CRITICAL();
  delay_us(410);
  return presence;
}

static void ow_write_bit(uint8_t bit)
{
  taskENTER_CRITICAL();
  ow_low();
  if (bit) { delay_us(6);  ow_release(); delay_us(64); }
  else     { delay_us(60); ow_release(); delay_us(10); }
  taskEXIT_CRITICAL();
}

static uint8_t ow_read_bit(void)
{
  uint8_t bit;
  taskENTER_CRITICAL();
  ow_low();
  delay_us(6);
  ow_release();
  delay_us(9);
  bit = (ow_read_pin() == GPIO_PIN_SET);
  taskEXIT_CRITICAL();
  delay_us(55);
  return bit;
}

static void ow_write_byte(uint8_t byte)
{
  for (int i = 0; i < 8; i++) { ow_write_bit(byte & 0x01); byte >>= 1; }
}

static uint8_t ow_read_byte(void)
{
  uint8_t byte = 0;
  for (int i = 0; i < 8; i++) { if (ow_read_bit()) byte |= (1 << i); }
  return byte;
}

int16_t ds18b20_read_centi(void)
{
  if (!ow_reset()) return DS18B20_NOT_FOUND;
  ow_write_byte(0xCC);          /* Skip ROM (one sensor on the bus) */
  ow_write_byte(0x44);          /* Start conversion */
  osDelay(750);                 /* 12-bit conversion: up to 750 ms, other tasks run meanwhile */

  if (!ow_reset()) return DS18B20_NOT_FOUND;
  ow_write_byte(0xCC);
  ow_write_byte(0xBE);          /* Read scratchpad */
  uint8_t lsb = ow_read_byte();
  uint8_t msb = ow_read_byte();

  int16_t raw = (int16_t)((msb << 8) | lsb);   /* units of 1/16 C */
  return (int16_t)((raw * 100) / 16);
}
