# Hardware logs scorecard

Results of `HARDWARE_LOGS_PLAN.md` and `HARDWARE_LOGS_PLAN_ADDENDUM1.md`, run on 11 November 2026 on one B-L072Z-LRWAN1. The predictions were committed before each run. Raw logs are in the folder `docs/secure_boot/`, in files whose names start with hwlog_, and the commands used are in `docs/secure_boot/hwlog_commands.txt`. Misses are kept in the table.

## Scorecard

| # | Prediction | What the log shows | Outcome |
|---|---|---|---|
| Baseline | Locked board reads RDP 0xBB and write protection on sectors 0 to 5 | RDP 0xBB, WRPOT0 to WRPOT5 active, WPRMOD 0, BFB2 0, BOR off. Boot log: `RDP level 1, OPTR=0x807000BB, WRPROT1=0x0000003F`, v1.3.1 started | Hit |
| P0 | At Level 1 a debugger write to slot B is refused | `Erasing internal memory sector 768`, then `Error: failed to erase memory`. The tool stopped at the erase, so nothing was written | Hit |
| P6 | After going back to Level 0, flash and EEPROM read as erased | RDP 0xAA. The first 64 bytes of the bootloader area, slot A and slot B, and the 16-byte anti-rollback record all read `00000000`. Only these starts were read, not all 192 KB | Hit (sampled) |
| P8 | Write protection on sectors 0 to 5 survives the regression, so the bootloader cannot be reflashed until it is cleared | WRPOT0 to WRPOT5 still active after the regression. Flashing the bootloader failed with `failed to erase memory` | Hit |
| P7 | At Level 0 the bootloader prints a warning and continues | `WARNING: RDP level 0, a debugger can read and change flash`, then `option-byte check passed`, then the app started | Hit |
| P1 | A flipped-bit update is refused at the hash check, slot B is erased, slot A still boots v1.1.0 | `update REJECTED, slot B erased, slot A untouched (SHA-256 MISMATCH)` in 645 ms. Slot B read back as zeros. v1.1.0 started | Hit |
| P1b | `bad130` is refused at the signature check | Refused at the hash check, `SHA-256 MISMATCH`, in 645 ms. Slot B erased, slot A untouched | **Miss** on the named check. The refusal itself happened |
| P3 | After a USB pull during the copy, the next boot completes the install by itself, every time | Three cuts, three recoveries, see below | Hit (3 of 3) |
| P2 | An older signed image is refused, slot A untouched | With the minimum at v1.3.1, `old110` (v1.1.0): `update REJECTED ... (older than the minimum allowed version)` in 2,335 ms. Slot B read as zeros after the reset (saved). A read-back before the reset showed the old image header, but a second read-back used the same file name and overwrote it, so the before-state exists only in my terminal output. v1.3.1 kept running | Hit |
| P4 | After a cut, reset to first app output takes 15 to 25 s | Replug to app output was about 47.5 s in cuts 2 and 3. The copy alone took about 43 s | **Miss** |
| P4b | The recopy takes 45 to 50 s from the banner to the app | About 47.5 s from the port reopening. The banner scrolled past before the port reopened, so the true figure is a little longer | Hit, borderline |
| P5 | A normal boot takes about 2 to 4 s from the reset line to the app | 2.5 s, reset line to app start, in every boot log | Hit |

## The three power cuts

The pull was by hand, so each cut fell later than aimed. Positions are interpolated from the log timestamps at about 6.9 s per 8,192 bytes.

| Cut | Image | Last `copied` line before the pull | Pull about | Result |
|---|---|---|---|---|
| 1 | `update120` v1.2.0 | 24,704 / 50,580 | 54% | The next boot showed v1.2.0 with minimum v1.2.0 and no update line. The recovery itself was not logged to its end, because I stopped the logger too early |
| 2 | `update130` v1.3.0 | 8,320 / 50,580 | 27% | Copy restarted from the beginning, `update installed`, v1.3.0 started, minimum raised to v1.3.0 |
| 3 | `update131` v1.3.1 | 41,088 / 50,580 | 89% | Copy restarted from the beginning, `update installed`, v1.3.1 started, minimum raised to v1.3.1 |

## Key rotation and re-lock

After the update tests I rotated the link keys (the frame counter had been reset by the mass erase, which would reuse keystreams with the old keys), installed the rebuilt app as v1.3.2 through slot B at Level 0, updated the Feather receiver, and set read-out protection Level 1 again. These predictions were written in the chat before each step. They were not committed to git first.

| # | Prediction | What the log shows | Outcome |
|---|---|---|---|
| K1 | The rebuilt app stays 50,068 bytes, the signed image is 50,580 bytes, and the hash is new | Build succeeded. 50,068 and 50,580 bytes. Signed image hash ce0da68c against ac778740 for update131 | Hit |
| K2 | 25 to 32 bytes differ from the old app, all in one small region | 32 bytes differ, all between file positions 49,589 and 49,620, where the two 16-byte keys sit | Hit, at the top of the range |
| K3 | Uploading the new sketch to the Feather keeps its old stored replay floor | After the upload it printed "No stored replay floor (fresh flash)" | **Miss** |
| K4 | With the new keys the Feather verifies readings within about 11 s of the app starting | Counters 1501 to 1517 verified about 1 s after the app started. Counter 1508 was sent by the board but is not in the Feather output, and I do not know why | Hit |
| K5 | The v1.3.2 install reports about 15.6 s while the PC clock shows about 43 s | `update installed in 15548 ms`. PC clock 42.7 s | Hit. The discrepancy is now seen on four installs |
| L1 | The lock script's precondition check passes | Level 0, sectors 0 to 5 protected, WPRMOD 0, BFB2 0, all preconditions met | Hit |
| L2 | Level 1 is set and reads back as 0xBB | After a typed confirmation: OPTR=0x807000BB, "Read-out protection is now LEVEL 1" | Hit |
| L3 | The locked board boots v1.3.2, the option-byte check passes, with no warning | RDP level 1, OPTR=0x807000BB, WRPROT1=0x0000003F, check passed, no warning. Image v1.3.2 verified, minimum v1.3.2, no copy lines. Frame counter resumed at 2101 and no counter repeated | Hit |
| L4 | A debugger read of slot A is refused at Level 1 | `failed to read the requested memory content` | Hit |
| L5 | A debugger read of the anti-rollback EEPROM record is refused at Level 1 | The same error, in a separate command | Hit |

## Misses and what they mean

- **P1b:** my inference that `bad130` carried a bad signature was wrong. The bootloader refuses it at the hash check. I did not find out how `bad130` differs from `update130` beyond the first differing bytes.
- **P4:** my estimate of 15 to 25 s came from the README figure of a 15.5 s install, which the logs do not support (see below).
- **K3:** I expected the Feather's stored replay floor to survive a sketch upload and planned a step to clear it. It did not survive, so the step was not needed. I do not know why, and I did not test whether the floor is stored in a region the upload erases.

## An unexplained discrepancy

The bootloader prints `update installed in 15614 ms` (cut 2) and `15595 ms` (cut 3). The timestamps on my PC put the same copy at about 42.7 s in both cuts, from `copied 128` to `copied 50580`. The SHA-256 and signature timings agree with the PC clock, so the mismatch is specific to the update timer. I do not know the cause. The README figure for the install time was not checked against a wall clock and needs correcting once the cause is understood.

## Deviations and limits

- The first boot after the write protection was set again was not logged. The board booted by itself before the logger was running.
- The first boot of the P1b log shows the banner twice, 61 ms apart. This was probably a second press of the reset button, but I did not verify that.
- One block of board output was pasted into the wrong terminal. PowerShell rejected every line, but a single echo line that appends to the commands file did run there. It appended text in another encoding, and the file then registered as binary data instead of text. I rebuilt the commands file from the same lines: null bytes and the byte-order mark removed, a duplicated P3 cut 1 line dropped, and two missing entries added (the state check after cut 1 and the first boot at Level 0). The damaged original is kept in a local backup.
- Pulling USB removes power and the serial port together, and brown-out reset is off in the factory setting. This is a hard pull, not a controlled supply drop or a slow brown-out.
- All update tests ran at Level 0, as stated in each log. One board, one cable, three hand-timed cuts.
- Slot B was only read back at the start of the region (64 bytes).
- The first boot log of the locked board, taken after unplugging USB, did not capture the option-byte line, because the serial port reopened after the banner had printed. A second capture, started the same way but using the reset button, caught it.
- The locked final boot reset log holds two logger sessions, because the logger appends to its file and I started it twice. The second session begins after the boot lines.
- The first locked read-back command stopped at its first error, so the EEPROM read was run as a separate command.
- The Feather's console lines were pasted by hand from the Arduino Serial Monitor and are not a saved log.

## Still to do

- Find the cause of the timer discrepancy and correct the README.
- Find out why counter 1508 did not reach the Feather.

## AI assistance

The plan, the addendum, the serial logger and this scorecard were written with Claude's help (AI-assisted). I ran every command on my own board and pasted the raw output.
