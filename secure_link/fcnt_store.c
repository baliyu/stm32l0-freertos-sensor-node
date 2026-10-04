/* fcnt_store.c - see fcnt_store.h */
#include "fcnt_store.h"

/* Returns 1 and the value if slot k holds a valid (value, ~value) pair */
static int slot_read(const fcnt_store *s, uint8_t k, uint32_t *v)
{
  uint32_t a = s->be->read(2U * k);
  uint32_t b = s->be->read(2U * k + 1U);
  if (b != ~a) return 0;
  *v = a;
  return 1;
}

/* Write a new reservation into the slot that does NOT hold the newest one,
 * then read it back. Only after this succeeds may values below it be used. */
static int reserve(fcnt_store *s, uint32_t limit)
{
  uint8_t k = (uint8_t)(s->newest ^ 1U);
  uint32_t check;

  if (s->be->write(2U * k, limit) != 0) return -1;
  if (s->be->write(2U * k + 1U, ~limit) != 0) return -1;
  if (!slot_read(s, k, &check) || check != limit) return -1;

  s->newest = k;
  s->reserved = limit;
  return 0;
}

int fcnt_store_init(fcnt_store *s, const fcnt_backend *be)
{
  uint32_t v0 = 0, v1 = 0;
  int ok0, ok1;

  s->be = be;
  ok0 = slot_read(s, 0, &v0);
  ok1 = slot_read(s, 1, &v1);

  /* Consecutive reservations always differ by exactly FCNT_BLOCK, so:
   *  - both slots valid: resume at the higher one
   *  - one slot valid:   the other was damaged (power cut or corruption) and may
   *                      have held a limit up to FCNT_BLOCK higher, so resume
   *                      FCNT_BLOCK above the survivor. Skips values, never reuses.
   *  - neither valid:    fresh storage, start from the beginning */
  if (ok0 && ok1)  { s->newest = (uint8_t)((v1 > v0) ? 1U : 0U); s->next = (v1 > v0) ? v1 : v0; }
  else if (ok0)    { s->newest = 0; s->next = v0 + FCNT_BLOCK; }
  else if (ok1)    { s->newest = 1; s->next = v1 + FCNT_BLOCK; }
  else             { s->newest = 1; s->next = 0; }        /* first write goes to slot 0 */

  if (s->next == 0) s->next = 1;          /* 0 is never sent: receivers start at last=0 */
  s->reserved = s->next;                  /* nothing usable until we reserve */

  return reserve(s, s->next + FCNT_BLOCK);
}

int fcnt_store_next(fcnt_store *s, uint32_t *out)
{
  if (s->next >= s->reserved)
  {
    if (reserve(s, s->next + FCNT_BLOCK) != 0) return -1;
  }
  *out = s->next++;
  return 0;
}
