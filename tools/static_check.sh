#!/usr/bin/env bash
# tools/static_check.sh - static checks (cppcheck and clang-tidy) on the host-compilable product code.
# Exits with an error if either tool finds anything.
#
#   bash tools/static_check.sh
#
# Run it from anywhere inside the repository. CI runs the same script (job static-analysis).
# Checked: bootloader sha256, image, rollback, update and ob_check; secure_link aes128, aes_cmac,
# secure_link and fcnt_store. Not checked: bootloader/main.c, flash_l0.c and nv_eeprom.c (they need
# device headers), Core/, third_party/ and the tests.
# The clang-tidy checks are listed in .clang-tidy, which also gives the reason for every disabled
# check. The cppcheck variableScope note in sha256.c is suppressed below (style only).
# Needs cppcheck and clang-tidy on the PATH (sudo apt install cppcheck clang-tidy).
set -euo pipefail
cd "$(dirname "$0")/.."
UECC=bootloader/third_party/micro-ecc
DEFS="-DuECC_SUPPORTS_secp160r1=0 -DuECC_SUPPORTS_secp192r1=0 -DuECC_SUPPORTS_secp224r1=0 -DuECC_SUPPORTS_secp256k1=0 -DuECC_SUPPORTS_secp256r1=1 -DuECC_SUPPORT_COMPRESSED_POINT=0"
BL="bootloader/sha256.c bootloader/image.c bootloader/rollback.c bootloader/update.c bootloader/ob_check.c"
SL="secure_link/aes128.c secure_link/aes_cmac.c secure_link/secure_link.c secure_link/fcnt_store.c"

echo "--- cppcheck ---"
# shellcheck disable=SC2086
cppcheck --enable=warning,style,performance,portability --inconclusive -q --error-exitcode=1 \
  --suppress=missingIncludeSystem --suppress='*:*/third_party/*' --suppress=variableScope:bootloader/sha256.c \
  $DEFS -I bootloader -I $UECC $BL
# shellcheck disable=SC2086
cppcheck --enable=warning,style,performance,portability --inconclusive -q --error-exitcode=1 \
  --suppress=missingIncludeSystem -I secure_link $SL
echo "cppcheck: no findings"

echo "--- clang-tidy ---"
# shellcheck disable=SC2086
clang-tidy -quiet $BL -- -std=gnu11 $DEFS -I bootloader -I $UECC
# shellcheck disable=SC2086
clang-tidy -quiet $SL -- -std=c99 -I secure_link
echo "clang-tidy: no findings"
