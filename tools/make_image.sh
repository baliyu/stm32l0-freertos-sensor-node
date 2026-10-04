#!/bin/sh
# make_image.sh - turn a built app ELF into a slot A image.
#   sh tools/make_image.sh <app.elf> <version> [out-name]
# Example:
#   sh tools/make_image.sh build/Release/l072_blinky.elf 1.0.0
set -e
ELF="$1"; VER="$2"; OUT="${3:-images/slotA}"
if [ -z "$ELF" ] || [ -z "$VER" ]; then
  echo "usage: sh tools/make_image.sh <app.elf> <version> [out-name]" >&2; exit 1
fi
mkdir -p "$(dirname "$OUT")"
arm-none-eabi-objcopy -O binary "$ELF" "$OUT.app.bin"
python3 "$(dirname "$0")/mkimage.py" "$OUT.app.bin" --version "$VER" -o "$OUT"
