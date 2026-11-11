# Hardware logs plan, addendum 1

Written after the read-only pre-flight and before any update test. The predictions in `HARDWARE_LOGS_PLAN.md` are unchanged. This addendum fixes the image set and adds predictions P1b, P7 and P8.

## What the pre-flight found (read-only, 11 November 2026)

- The firmware version is stored at header offset 16 (`major<<24 | minor<<16 | patch`). Versions below were read from the files with a short Python script.
- Slot placement was read from the first lines of each `.hex` file (0x08006000 is slot A, 0x08018000 is slot B).

| Image | Version | Signed | Placement | Planned use |
|---|---|---|---|---|
| `slotA` | 1.1.0 | yes | slot A | baseline boot |
| `update120` | 1.2.0 | yes | slot B | P3, cut 1 |
| `update130` | 1.3.0 | yes | slot B | P3, cut 2 |
| `update131` | 1.3.1 | yes | slot B | P3, cut 3 |
| `old110` | 1.1.0 | yes | slot B | P2, after 1.3.1 has installed |
| `bad130` | 1.3.0 | yes | slot B | P1b |

## P1 and `bad130`

P1 is a flipped-bit update. The existing `bad130.bin` is not that image. `cmp -l` against `update130.bin` shows its first difference at byte 65 of the file, which is header offset 64, the start of the signature field. I have only seen the first five differing bytes, so this is an inference: `bad130` is `update130` with a wrong signature.

- P1 stays as written and will use a new image made with `tools/mkimage.py --tamper` (one bit flipped in the app area), so that the hash no longer matches.
- **P1b (new):** `bad130` is refused at the signature check, not the hash check. Slot B is erased and slot A still boots v1.1.0.

## New predictions

| # | Prediction | Basis | Proved wrong if |
|---|---|---|---|
| P1b | `bad130` is refused with a signature reason, slot B is erased, slot A is unchanged | Header difference at the signature field | It installs, or the refusal names the hash |
| P7 | At Level 0 every boot prints the RDP level 0 warning and continues, because this is a development build | `ob_check` source and README (OB_REQUIRE_RDP1 is 0) | The bootloader halts at Level 0 |
| P8 | After the Level 1 to Level 0 regression, the write-protection bits for sectors 0 to 5 are still set, so the bootloader sectors need the protection cleared before they can be reflashed | An ST article says option bytes are kept in the regression (general, not STM32L0 specific; not confirmed for this chip) | WRPOT0 to WRPOT5 read as not active after the regression |

P6 (everything reads as erased) stays as written. P8 and P6 may disagree. If WRP survives, I expect P6 to be a miss for sectors 0 to 5, and that will be recorded as a miss.

## Cut points for P3

The bootloader prints `[BOOT]   copied N / M bytes` while it copies. A cut is defined by the last such line seen before the pull, not by a stopwatch. Targets: about 20%, 50% and 80% of M. The log records the actual last line. A hand pull is not precise, so a cut that lands outside the target is still a valid test and is recorded as it fell.

## Logging

Each log records the command, the raw output, the date and time, the RDP level (from the `[BOOT] option bytes` line) and the sha256 of every image used. Serial output is captured with timestamps.

## AI assistance

This addendum was written with Claude's help (AI-assisted). The runs are done by me on my own board.
