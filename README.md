# STM32L0 FreeRTOS Sensor Node

Firmware for the ST **B-L072Z-LRWAN1** (STM32L072CZ, ARM Cortex-M0+, SX1276 LoRa), built with STM32CubeMX and STM32CubeIDE.

## Progress
- [x] ** GPIO + UART bring-up — LED toggling and serial output over the ST-LINK virtual COM port (115200 baud)
- [x] ** FreeRTOS (CMSIS-RTOS v2) with independent LED and UART tasks
- [x] ** DS18B20 sensor task with queue-based logging
- [ ] ** Interrupt-driven events, mutex-protected UART, LoRa transmission task
- [ ] ** Watchdog, low-power tickless idle, CMake/GCC command-line build

## Lessons learned
- Code placed after the closing brace of `while (1)` never executes; application code must sit inside the loop, within CubeMX `USER CODE` markers so regeneration preserves it.
- Leading whitespace in `.gitignore` silently breaks pattern matching; build output was committed until the file was corrected and tracked files removed with `git rm --cached`.
- With FreeRTOS, the HAL needs its own timebase (TIM21) because the scheduler takes over SysTick; after `osKernelStart()` the `while (1)` loop in `main()` is never reached.
- The TIM2 prescaler set in CubeMX didn't save (generated as 0), which would have made every 1-Wire delay 32x too short; fixed by re-initialising the timer at 1 MHz inside a USER CODE block so regeneration can't undo it.
- Producer–consumer with an RTOS queue: the sensor task produces readings, the logger blocks on the queue (osWaitForever) and uses no CPU while waiting.
- A binary semaphore holds at most one pending signal, so repeated button presses during a reading trigger only one extra reading.
- CubeMX creates semaphores with an initial count of 1; the task drains it at start-up so only real presses count.
- The ISR does the minimum (debounce check, release semaphore); the work happens in the task.
- A watchdog that is fed from a timer or from one task can mask a hung task. Each task sets an event flag; a supervisor task feeds the IWDG only when all flags arrive, so any single stuck task triggers a reset. Tested with a deliberate hang (`SIMULATE_SENSOR_HANG`).
## Tools
STM32CubeMX · STM32CubeIDE 2.x · STM32 HAL · Git · PuTTY
