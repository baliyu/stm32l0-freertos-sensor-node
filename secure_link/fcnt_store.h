/* fcnt_store.h - frame counter that survives resets, without reusing a value.
 *
 * Why: if the counter restarts after a reset, the sender reuses (key, counter)
 * pairs. That repeats the AES-CTR keystream (leaking plaintext differences)
 * and makes the receiver reject the node as a replay.
 *
 * How: "reserve before use". Non-volatile storage holds a limit R; every
 * counter handed out is below R. On boot the node resumes AT R, so values it
 * may already have used are never repeated (some values are skipped instead).
 * The limit is raised in blocks of FCNT_BLOCK to limit memory wear.
 *
 * Power-fail safety: two slots, each stored as (value, ~value). Writes go to
 * the older slot, so a write interrupted by power loss leaves the newer slot
 * intact, and a half-written slot fails the ~value check and is ignored.
 *
 * Storage access goes through a small backend so the same logic runs on the
 * STM32 (data EEPROM) and in the PC tests (a RAM array).
 */
#ifndef FCNT_STORE_H
#define FCNT_STORE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FCNT_BLOCK 100U          /* counters reserved per storage write */
#define FCNT_WORDS 4U            /* storage needed: 2 slots x 2 words */

typedef struct {
  uint32_t (*read)(uint32_t index);                 /* read word 0..3 */
  int      (*write)(uint32_t index, uint32_t value); /* 0 = ok */
} fcnt_backend;

typedef struct {
  const fcnt_backend *be;
  uint32_t next;                 /* next counter value to hand out */
  uint32_t reserved;             /* every value < reserved is covered by storage */
  uint8_t  newest;               /* slot holding the current reservation */
} fcnt_store;

/* Load from storage and make the first reservation. 0 = ok, -1 = storage error. */
int fcnt_store_init(fcnt_store *s, const fcnt_backend *be);

/* Hand out the next counter. 0 = ok; -1 = storage error (do NOT transmit). */
int fcnt_store_next(fcnt_store *s, uint32_t *out);

#ifdef __cplusplus
}
#endif

#endif
