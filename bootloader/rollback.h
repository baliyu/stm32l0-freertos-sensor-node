/* rollback.h - anti-rollback: the lowest firmware version the bootloader will
 * still start, kept in non-volatile memory and only ever raised.
 *
 * Why: every image you ever signed stays validly signed forever. Without this,
 * an attacker could reinstall an old, genuine image with a known bug.
 *
 * Storage: two copies, each (value, ~value). A new minimum is written to copy
 * A, then copy B. A power cut can only damage the copy being written; the
 * other still holds a valid value, and the larger valid value wins.
 *
 * Access goes through a small backend so the same logic runs on the STM32
 * (data EEPROM) and in the PC tests (a RAM array).
 */
#ifndef ROLLBACK_H
#define ROLLBACK_H

#include <stdint.h>

#define ROLLBACK_WORDS 4U               /* storage needed: 2 copies x 2 words */

typedef struct {
  uint32_t (*read)(uint32_t index);                 /* read word 0..3 */
  int      (*write)(uint32_t index, uint32_t value); /* 0 = ok */
} nv_backend;

/* Current minimum version (0 if nothing valid has been stored yet). */
uint32_t rollback_min_version(const nv_backend *be);

/* Make both copies hold max(current minimum, version). Writes nothing when
 * they already do, so it is safe (and wear-free) to call on every boot; when
 * one copy is stale or damaged it is repaired. Returns 0 on success, -1 if a
 * storage write failed. */
int rollback_raise(const nv_backend *be, uint32_t version);

#endif
