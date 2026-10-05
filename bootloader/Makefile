# Bootloader build. Uses the CMSIS headers and startup file that CubeMX already
# put in the main project. Run from this folder:  make
PREFIX  = arm-none-eabi-
CC      = $(PREFIX)gcc
OBJCOPY = $(PREFIX)objcopy
SIZE    = $(PREFIX)size

ROOT    ?= ..
CMSIS_DEV  ?= $(ROOT)/Drivers/CMSIS/Device/ST/STM32L0xx/Include
CMSIS_CORE ?= $(ROOT)/Drivers/CMSIS/Include
STARTUP    ?= $(ROOT)/startup_stm32l072xx.s
SYSTEM     ?= $(ROOT)/Core/Src/system_stm32l0xx.c

UECC    = third_party/micro-ecc
# Only P-256, no compressed points: smaller code. Verification needs no RNG.
UECC_DEFS = -DuECC_SUPPORTS_secp160r1=0 -DuECC_SUPPORTS_secp192r1=0 -DuECC_SUPPORTS_secp224r1=0 \
            -DuECC_SUPPORTS_secp256k1=0 -DuECC_SUPPORTS_secp256r1=1 -DuECC_SUPPORT_COMPRESSED_POINT=0

MCU     = -mcpu=cortex-m0plus -mthumb
CFLAGS  = $(MCU) -std=gnu11 -Os -g3 -Wall -Wextra -ffunction-sections -fdata-sections \
          -DSTM32L072xx -I. -I$(UECC) -I$(CMSIS_DEV) -I$(CMSIS_CORE) $(UECC_DEFS)
LDFLAGS = $(MCU) -T bootloader.ld -Wl,--gc-sections -Wl,-Map=bootloader.map \
          -Wl,--print-memory-usage --specs=nano.specs --specs=nosys.specs

OBJS    = build/main.o build/image.o build/sha256.o build/rollback.o build/nv_eeprom.o \
          build/update.o build/flash_l0.o build/uECC.o build/system.o build/startup.o

all: bootloader.elf bootloader.bin

build:
	mkdir -p build

build/main.o: main.c image.h pubkey.h rollback.h nv_eeprom.h update.h flash_l0.h | build
	$(CC) $(CFLAGS) -c -o $@ $<
build/image.o: image.c image.h sha256.h | build
	$(CC) $(CFLAGS) -c -o $@ $<
build/sha256.o: sha256.c sha256.h | build
	$(CC) $(CFLAGS) -c -o $@ $<
build/rollback.o: rollback.c rollback.h | build
	$(CC) $(CFLAGS) -c -o $@ $<
build/nv_eeprom.o: nv_eeprom.c nv_eeprom.h rollback.h | build
	$(CC) $(CFLAGS) -c -o $@ $<
build/update.o: update.c update.h image.h | build
	$(CC) $(CFLAGS) -c -o $@ $<
build/flash_l0.o: flash_l0.c flash_l0.h update.h image.h | build
	$(CC) $(CFLAGS) -c -o $@ $<
# Third-party code: same flags, minus a warning its assembly helpers trigger
build/uECC.o: $(UECC)/uECC.c | build
	$(CC) $(CFLAGS) -Wno-unused-parameter -c -o $@ $<
build/system.o: $(SYSTEM) | build
	$(CC) $(CFLAGS) -c -o $@ $<
build/startup.o: $(STARTUP) | build
	$(CC) $(CFLAGS) -c -o $@ $<

bootloader.elf: $(OBJS) bootloader.ld
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	$(SIZE) $@

bootloader.bin: bootloader.elf
	$(OBJCOPY) -O binary $< $@

pubkey.h:
	@echo "bootloader/pubkey.h missing: run  python3 tools/gen_signing_key.py  from the repo root" >&2
	@false

clean:
	rm -rf build bootloader.elf bootloader.bin bootloader.map
