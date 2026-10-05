#!/usr/bin/env python3
"""make_eeprom_reset.py - DEVELOPMENT ONLY: a .hex that clears the bootloader's
anti-rollback record in data EEPROM (16 zero bytes at 0x08080100), so older
test versions can boot again.

This works only because the debug port is still open. Once the board is locked
down (stage 6), nobody can do this, which is the point of locking it.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from mkimage import write_ihex  # noqa: E402  (same folder)

ROLLBACK_EEPROM_ADDR = 0x08080100
out = sys.argv[1] if len(sys.argv) > 1 else "images/reset_min_version.hex"
os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
write_ihex(out, bytes(16), ROLLBACK_EEPROM_ADDR)
print(f"written {out}: 16 zero bytes at 0x{ROLLBACK_EEPROM_ADDR:08X} (minimum version -> none)")
