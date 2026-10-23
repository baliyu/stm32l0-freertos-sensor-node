# Attacker's view of the bootloader

A reverse-engineering note on my own bootloader. Each finding is marked as read directly from the binary, or inferred.

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

## Inferred (not verified)

- **`0x08001cbc` is the ECDSA P-256 verify routine.** It is called after 64 bytes are copied from the header, and the constants at `0x080027a0` look like the standard P-256 values. I did not check them against a reference table.
- **What RDP level 0 allows.** This comes from my own README tests (a debugger read the EEPROM record at level 0, and write protection was removed with a debugger during the lockdown test). I have not repeated those tests for this note, and have not yet checked the reference manual (RM0376).

## What an attacker learns

1. **The signature chain is the main defence.** In this binary every failed check ends in a halt, and I found no branch around them. That is read, not proven.
2. **The chain only holds if the bootloader and the key cannot be rewritten.** Both sit in flash. At RDP level 0, a debugger could probably remove write protection and replace them (inferred). So the lockdown matters more than the code.
3. **This build only warns at level 0** (`OB_REQUIRE_RDP1 = 0`). A production build would halt.
4. **From my README, not re-tested here:** level 1 blocks debugger reads, but bypasses of STM32 read-out protection have been published. BFB2=1 can boot straight past the bootloader. An app running with full privileges can rewrite the option bytes and the EEPROM.

## Next

- Build with `OB_REQUIRE_RDP1 = 1` for production.
- Look up RDP level 0 and option-byte rewrite behaviour in RM0376 and cite the section.
- Check the curve constants and the verify routine against the micro-ecc source.

## Limits

One binary, one evening, static analysis only. Nothing in this note was run on hardware.
