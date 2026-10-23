# Attacker's view of the bootloader

A reverse-engineering note on my own bootloader. Each finding is marked as read directly from the binary, checked against the STM32L0 reference manual, or inferred.

## What I did

I loaded `bootloader/bootloader.bin` (10,324 bytes, SHA-256 starting `2a5725b5`, built from commit `281b655`, a development build) into Ghidra 12.1.4 as a raw ARM Cortex-M0+ binary at `0x08000000`, and read the code as an attacker holding only the binary would. I ran Ghidra and checked the strings, addresses and key bytes myself. An AI assistant (Claude) helped me interpret the disassembly. Anything it suggested that I could not check is listed under "Inferred".

To reproduce: `sha256sum bootloader/bootloader.bin`, then import in Ghidra as Raw Binary, language ARM:LE:32:Cortex, base address `0x08000000`.

## Read directly from the binary

| # | Finding | Where |
|---|---------|-------|
| 1 | Reset path: initial stack pointer `0x20005000`, reset vector `0x08001fa5`. Startup copies initialised data, zeroes RAM, then calls the main routine at `0x080005d0`. | vector table; `0x08001fa4` |
| 2 | Option-byte check: reads FLASH_OPTR (`0x4002201C`) and WRPROT1 (`0x40022020`) and tests four conditions, each with its own message (sectors 0-5 not write-protected, WPRMOD=1, BFB2=1, RDP level 0). The first three halt the boot. RDP level 0 only prints a warning, because the check is called with its second argument fixed at 0. | strings `0x08002114` to `0x080021d3`; call at `0x080006cc` |
| 3 | Boot order: slot B (`0x08018000`) is checked first for an update. Then slot A (`0x08006000`): header, SHA-256, signature, vector table, minimum version, jump. | status table `0x08002654`; main routine |
| 4 | Every non-zero status prints a message and loops forever ("refusing to start the application"). | `0x08002014`; main routine |
| 5 | SHA-256 is present: initial values at `0x08000d88`, round constants at `0x08002670`. The digest is compared with the 32 bytes at header offset `0x14`. | listing |
| 6 | The trusted public key is the 64 bytes at `0x08002613`. They are identical, byte for byte, to `bootloader/pubkey.h`. The key id `e4edbc95` is also in the binary. | `0x08002613`; `0x080020d8` |
| 7 | Handover: writes the vector table register (`0xE000ED08`) with `0x08006200`, loads the stack pointer from the image, and jumps. | `0x08000a4a` to `0x08000a58` |
| 8 | The flash unlock keys are in the binary: the PECR keys (`0x89ABCDEF`, `0x02030405`) at `0x08001010` and the program-memory keys (`0x8C9DAEBF`, `0x13141516`) at `0x080011d8`. The option-byte keys (`0xFBEAD9C8`, `0x24252627`) are not present as constants. | byte search of the file; listing |

## Checked against the reference manual (RM0376 Rev 7)

| # | What the manual says | Where |
|---|----------------------|-------|
| A | At RDP level 0 there is no protection: with no other protection enabled, flash, data EEPROM and the option bytes can be read and written without restriction. | section 3.4.1, p. 96 |
| B | At level 1, with the debugger connected, flash and data EEPROM cannot be read or written (a read causes a hard fault). | section 3.4.1, p. 96; hard faults, p. 102 |
| C | Writing the first option byte to go from level 1 back to level 0 triggers a mass erase of the flash and the data EEPROM. | section 3.4.1, p. 96; mass erase, p. 92 |
| D | Level 2 is permanent: debug is disabled and the option bytes can no longer be written. | section 3.4.1, p. 96 |
| E | If level 1 is set while a debugger was active, a power-on reset is needed, not just a system reset. | section 3.4, p. 95 |
| F | Writing flash, data EEPROM or the option bytes needs an unlock key sequence. Without it, a write-protection error is raised. A wrong key value causes a hard fault, and the lock stays shut until the next system reset. | p. 101, p. 102, p. 110 |
| G | The unlock order is: PECR keys first (`0x89ABCDEF`, `0x02030405`), then the option-byte keys (`0xFBEAD9C8`, `0x24252627`), which only work once PECR is unlocked. Program memory has its own keys (`0x8C9DAEBF`, `0x13141516`). The control register starts locked. | pp. 108 to 113 |

These agree with my own hardware tests in the README: debugger reads failed at level 1, and going back to level 0 erases everything.

**One point where the manual disagrees with itself.** Section 3.4.1 (p. 96) says that at level 1 the option bytes can still be read and written. The error-flag list (p. 101) says a write to the option bytes raises a write-protection error when the RDP option byte is set and the device is in debug mode. I have not tested which is true on the chip, so I treat option-byte writes under a debugger at level 1 as unresolved.

## Inferred (not verified)

- **`0x08001cbc` is the ECDSA P-256 verify routine.** It is called after 64 bytes are copied from the header, and the constants at `0x080027a0` look like the standard P-256 values. I did not check them against a reference table.
- **At level 0, a debugger could remove write protection and replace the bootloader.** The manual (A) says the option bytes are writable at level 0, and my lockdown test removed write protection with a debugger. I did not test replacing the bootloader afterwards.
- **Code running on the chip could unlock and rewrite the option bytes.** The manual describes unlocking as a plain register sequence with published keys (G), so any code that can write those registers can do it. I have not tested this on the chip.
- **The bootloader never rewrites option bytes.** Finding 8 shows it does not contain the option-byte keys as constants. Code could in principle build them another way, but I saw no sign of it.

## What an attacker learns

1. **The signature chain is the main defence.** In this binary every failed check ends in a halt, and I found no branch around them. That is read, not proven.
2. **The chain only holds if the bootloader and the key cannot be rewritten.** Both sit in flash. At level 0 the manual allows unrestricted writes to the option bytes (A), so the lockdown matters more than the code.
3. **The unlock keys are public.** They are in the reference manual, so write protection only stops accidents. It does not stop code that is allowed to write the registers.
4. **This build only warns at level 0** (`OB_REQUIRE_RDP1 = 0`). A production build would halt.
5. **At level 1 a debugger cannot read or write flash (B).** Whether it can still change the option bytes is unresolved (see above). The bootloader re-checks the option bytes on every boot either way.
6. **From my README, not re-tested here:** bypasses of STM32 read-out protection have been published. BFB2=1 can boot straight past the bootloader. An app running with full privileges can rewrite the option bytes and the EEPROM.

## Next

- Build with `OB_REQUIRE_RDP1 = 1` for production.
- Settle the level 1 option-byte write question on the chip.
- Check the curve constants and the verify routine against the micro-ecc source.

## Limits

One binary, one evening, static analysis plus a manual read. Nothing in this note was run on hardware.
