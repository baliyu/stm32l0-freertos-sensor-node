#!/bin/sh
# tools/rdp_level1.sh - secure boot stage 6: set read-out protection to Level 1
# (RDP = 0xBB) on the B-L072Z-LRWAN1, and nothing else.
#
#   sh tools/rdp_level1.sh --check   read and check only, changes nothing
#   sh tools/rdp_level1.sh           check, ask for confirmation, then set Level 1
#
# Guard rails:
#   - The only RDP value this script can ever write is 0xBB. Level 2 (0xCC) is
#     permanent (the debug port is disabled for good) and is never used here.
#   - It refuses to run unless the chip is at Level 0 AND the bootloader
#     sectors 0-5 are write-protected (WRPROT1 bits 0-5), WPRMOD=0 and BFB2=0,
#     i.e. exactly the state the stage 6 bootloader accepts.
#   - It reads the result back and stops loudly if it is not Level 1.
# Going back from Level 1 to Level 0 erases the whole flash and data EEPROM.
set -eu

CLI="${CLI:-/mnt/c/Program Files/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin/STM32_Programmer_CLI.exe}"
LOGDIR="docs/secure_boot"
TARGET_RDP="BB"                 # the only value this script writes

die() { echo "STOPPED: $*" >&2; exit 1; }

[ -x "$CLI" ] || die "CubeProgrammer CLI not found at: $CLI"
[ -d "$LOGDIR" ] || die "run from the repo root (no $LOGDIR folder here)"

# Read FLASH_OPTR and FLASH_WRPROT1 -> sets OPTR and WRP (8 hex digits each)
read_regs() {
  out=$("$CLI" -c port=SWD mode=UR reset=HWrst -r32 0x4002201C 0x08 2>&1 | tr -d '\r') \
    || die "CubeProgrammer could not read the registers:
$out"
  line=$(printf '%s\n' "$out" | grep -i '^0x4002201C *:' || true)
  [ -n "$line" ] || die "no register line in CubeProgrammer output:
$out"
  OPTR=$(printf '%s\n' "$line" | awk '{print toupper($3)}')
  WRP=$(printf '%s\n' "$line"  | awk '{print toupper($4)}')
  case "$OPTR$WRP" in
    *[!0-9A-F]*|"") die "could not parse '$line'" ;;
  esac
  [ ${#OPTR} -eq 8 ] && [ ${#WRP} -eq 8 ] || die "could not parse '$line'"
}

rdp_of()    { printf '%s' "$1" | cut -c7-8; }                  # low byte of OPTR
bit_set()   { [ $(( 0x$1 & $2 )) -ne 0 ]; }

read_regs
RDP=$(rdp_of "$OPTR")
echo "Current: FLASH_OPTR=0x$OPTR  FLASH_WRPROT1=0x$WRP  (RDP=0x$RDP)"

case "$RDP" in
  AA) echo "  RDP level 0 (no read-out protection)" ;;
  CC) die "chip reports Level 2 (0xCC). Nothing can be changed." ;;
  *)  die "chip is already at Level 1 (RDP=0x$RDP). Nothing to do." ;;
esac
[ $(( 0x$WRP & 0x3F )) -eq $(( 0x3F )) ] || die "bootloader sectors 0-5 are not all write-protected (WRPROT1=0x$WRP). Do stage 6.2 first."
echo "  bootloader sectors 0-5 write-protected"
if bit_set "$OPTR" 0x100;    then die "WPRMOD=1 (PCROP mode). Expected 0."; fi
echo "  WPRMOD=0 (write-protection mode)"
if bit_set "$OPTR" 0x800000; then die "BFB2=1 (boot from bank 2). Expected 0."; fi
echo "  BFB2=0 (boot from bank 1)"
echo "All preconditions met."

if [ "${1:-}" = "--check" ]; then
  echo "--check: nothing changed."
  exit 0
fi

cat << MSG

About to set read-out protection LEVEL 1 (RDP=0x$TARGET_RDP).
  - The debugger will no longer be able to read or write flash or data EEPROM.
  - Firmware can then only be changed by going back to Level 0, which ERASES
    the whole flash (bootloader, app) and the data EEPROM (frame counter,
    minimum version).
  - Level 2 is never used by this script.
MSG
printf 'Type LEVEL1 to continue, anything else to cancel: '
read -r answer
[ "$answer" = "LEVEL1" ] || die "cancelled, nothing changed."

STAMP=$(date +%Y%m%d_%H%M%S)
"$CLI" -c port=SWD mode=UR reset=HWrst -ob RDP=0x$TARGET_RDP 2>&1 | tr -d '\r' \
  | tee "$LOGDIR/stage6_3_set_rdp1_$STAMP.txt" || true

read_regs
RDP=$(rdp_of "$OPTR")
echo "After:   FLASH_OPTR=0x$OPTR  FLASH_WRPROT1=0x$WRP  (RDP=0x$RDP)" \
  | tee -a "$LOGDIR/stage6_3_set_rdp1_$STAMP.txt"
case "$RDP" in
  AA) die "still Level 0 - the option-byte write did not take effect." ;;
  CC) die "LEVEL 2 REPORTED. This script never writes 0xCC - stop and investigate." ;;
  BB) echo "Read-out protection is now LEVEL 1." ;;
  *)  echo "RDP=0x$RDP: the chip treats this as Level 1, but 0xBB was expected - check the log." ;;
esac
echo "Now unplug the USB cable for 5 s (power-on reset), plug it back in, reopen PuTTY."
