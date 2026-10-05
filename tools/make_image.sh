#!/bin/sh
# make_image.sh - turn a built app ELF into a signed slot A image.
#   sh tools/make_image.sh <app.elf> <version> [out-name]
# Example:
#   sh tools/make_image.sh build/Debug/l072_blinky.elf 1.0.0
# Signs with keys/signing_key.pem (create it with tools/gen_signing_key.py).
# Set SLOT=B to make an update image for slot B instead of slot A:
#   SLOT=B sh tools/make_image.sh build/Debug/l072_blinky.elf 1.2.0 images/update
set -e
ELF="$1"; VER="$2"; OUT="${3:-images/slotA}"
KEY="${KEY:-keys/signing_key.pem}"
SLOT="${SLOT:-A}"
if [ -z "$ELF" ] || [ -z "$VER" ]; then
  echo "usage: sh tools/make_image.sh <app.elf> <version> [out-name]" >&2; exit 1
fi
if [ ! -f "$KEY" ]; then
  echo "error: no signing key at $KEY - run: python3 tools/gen_signing_key.py" >&2; exit 1
fi
mkdir -p "$(dirname "$OUT")"
arm-none-eabi-objcopy -O binary "$ELF" "$OUT.app.bin"
python3 "$(dirname "$0")/mkimage.py" "$OUT.app.bin" --version "$VER" --key "$KEY" --slot "$SLOT" -o "$OUT"
