# Hardware logs plan: tamper, older image and a real power cut

Written before any run. Predictions are fixed here and will not be edited after the first measurement. Results go in a separate scorecard, with misses recorded.

## Why

The README claims three things were shown on hardware: a tampered update is refused, an older signed update is refused, and a USB power cut during an install recovers on the next boot. The raw logs for these are not all saved in `docs/secure_boot/`. The 1,682 power-cut points in the host tests are a simulation on a PC, not a real power cut. This plan produces the missing logs, or shows that a claim does not hold.

## Constraint

One board (B-L072Z-LRWAN1), currently at read-out protection Level 1 and write-protected. At Level 1 the debugger cannot write flash, so the update tests cannot run on the locked board. Plan: log the locked-board refusal first, go back to Level 0 (this mass-erases the flash and the data EEPROM), run the tests, then lock the board again. Every log from the update tests must state that the board was at Level 0.

## Predictions

| # | Prediction | Basis | Proved wrong if |
|---|---|---|---|
| P0 | At Level 1, a debugger write to slot B (0x08018000) is refused | README: no update path at Level 1 | The write succeeds |
| P1 | An update image with one flipped bit is refused at the hash check, slot B is erased, and slot A still boots the old version | Secure boot stages 2 and 5 | It installs, or the old version no longer boots |
| P2 | A correctly signed update older than the stored minimum version is refused with slot A untouched | Stages 4 and 5 | It installs, or slot A changes |
| P3 | After a USB pull at about 3 s, 8 s and 13 s into the copy, the next boot completes the install by itself and the app reports the new version, every time | Stage 5 and the simulated cuts | Any halt, any old version, or any case that needs reflashing |
| P4 | After a cut, the next boot takes 15 to 25 s from reset to the first app output | 15.5 s install plus about 2.2 s of checks | Outside that range |
| P5 | After a recovery, the next normal boot takes about 2 to 4 s, so slot B was cleared | Copy, compare, then clear B | The second boot reinstalls |
| P6 | After going back to Level 0, flash and data EEPROM read as erased and the board does nothing until reflashed | README: going back to Level 0 triggers a mass erase | Any old content remains |

## Procedure (order fixed)

1. P0 on the locked board. Save the output of the refused write.
2. Go back to Level 0. Read the flash and EEPROM and save the output (P6).
3. Reflash the bootloader and a signed slot A image. Hardware reset. Save the boot output.
4. P1, then P2, then P3 (three cuts), P4 and P5 measured on the same runs.
5. Lock the board again with the guarded script (`tools/rdp_level1.sh`). Save the check output.
6. Rotate the link keys, as the README requires after the counter state is lost.

## What each log must contain

The command used, the raw output, the date and time, the RDP level, the firmware versions and the sha256 of the image used. Cut times are measured from the serial line that announces the copy; if the bootloader prints no such line, the time is measured from reset and the log says so.

## Stop rules

If anything unexpected happens (a halt, a different version, a refused command), stop, save the output and report it. Do not retry silently and do not change the plan. A failed run is a result.

## Limits

One board, one USB cable, three cut times by hand. A hand-timed pull is not a controlled power cut. This does not test brown-out behaviour (brown-out reset is off in the factory setting), only a hard USB pull.

## AI assistance

This plan was written with Claude's help (AI-assisted). The runs are done by me on my own board.
