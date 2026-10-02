# STM32L0 FreeRTOS Sensor Node

Firmware for the ST **B-L072Z-LRWAN1** (STM32L072CZ, ARM Cortex-M0+, SX1276 LoRa), built with STM32CubeMX and STM32CubeIDE.

## Progress
- [x] **Week 1a:** GPIO + UART bring-up — LED toggling and serial output over the ST-LINK virtual COM port (115200 baud)
- [ ] **Week 1b:** FreeRTOS (CMSIS-RTOS v2) with independent LED and UART tasks
- [ ] **Week 2:** DS18B20 sensor task with queue-based logging
- [ ] **Week 3:** Interrupt-driven events, mutex-protected UART, LoRa transmission task
- [ ] **Week 4:** Watchdog, low-power tickless idle, CMake/GCC command-line build

## Lessons learned
- Code placed after the closing brace of `while (1)` never executes; application code must sit inside the loop, within CubeMX `USER CODE` markers so regeneration preserves it.
- Leading whitespace in `.gitignore` silently breaks pattern matching; build output was committed until the file was corrected and tracked files removed with `git rm --cached`.
- With FreeRTOS, the HAL needs its own timebase (TIM21) because the scheduler takes over SysTick; after `osKernelStart()` the `while (1)` loop in `main()` is never reached.
## Tools
STM32CubeMX · STM32CubeIDE 2.x · STM32 HAL · Git · PuTTY
