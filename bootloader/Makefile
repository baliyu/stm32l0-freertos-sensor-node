# Bootloader build (stage 1). Uses the CMSIS headers and startup file that
# CubeMX already put in the main project, so nothing extra is downloaded.
# Run from this folder:  make
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

ROOT    ?= ..
CMSIS_DEV  ?= $(ROOT)/Drivers/CMSIS/Device/ST/STM32L0xx/Include
CMSIS_CORE ?= $(ROOT)/Drivers/CMSIS/Include
STARTUP    ?= $(ROOT)/startup_stm32l072xx.s
SYSTEM     ?= $(ROOT)/Core/Src/system_stm32l0xx.c

MCU     = -mcpu=cortex-m0plus -mthumb
CFLAGS  = $(MCU) -std=gnu11 -Os -g3 -Wall -Wextra -ffunction-sections -fdata-sections \
          -DSTM32L072xx -I$(CMSIS_DEV) -I$(CMSIS_CORE)
LDFLAGS = $(MCU) -T bootloader.ld -Wl,--gc-sections -Wl,-Map=bootloader.map \
          -Wl,--print-memory-usage --specs=nano.specs --specs=nosys.specs

all: bootloader.elf bootloader.bin

bootloader.elf: main.c $(SYSTEM) $(STARTUP) bootloader.ld
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ main.c $(SYSTEM) $(STARTUP)
	$(SIZE) $@

bootloader.bin: bootloader.elf
	$(OBJCOPY) -O binary $< $@

clean:
	rm -f bootloader.elf bootloader.bin bootloader.map
