/* flash_l0.h - STM32L0 program-memory erase/program for the bootloader (no HAL). */
#ifndef FLASH_L0_H
#define FLASH_L0_H

#include "update.h"

extern const flash_ops flash_l0;

/* Set by main.c to print copy progress (may stay NULL) */
extern void (*flash_l0_progress)(uint32_t done, uint32_t total);

#endif
