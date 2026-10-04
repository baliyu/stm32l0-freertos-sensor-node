#!/bin/sh
# Copy the shared crypto code and your keys into the Arduino sketch folder.
# Arduino only compiles files that sit inside the sketch folder, so the Feather
# gets copies; secure_link/ stays the single source of truth (copies are git-ignored).
set -e
cd "$(dirname "$0")"
if [ ! -f secure_link_keys.h ]; then
  echo "secure_link_keys.h missing: run  python3 gen_keys.py  first" >&2
  exit 1
fi
cp aes128.c aes128.h aes_cmac.c aes_cmac.h secure_link.c secure_link.h secure_link_keys.h ../feather_receiver/
echo "copied crypto files and keys to ../feather_receiver/"
