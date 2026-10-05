/* test_rollback.c - the anti-rollback store, with a fake EEPROM in RAM.
 * Simulates power cuts in the middle of a write and corrupted copies. */
#include <stdio.h>
#include <string.h>
#include "rollback.h"

#define VER(a, b, c) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | (uint32_t)(c))

static uint32_t ee[ROLLBACK_WORDS];       /* erased L0 EEPROM reads 0 */
static int writes = 0;
static int fail_after = -1;               /* >= 0: writes beyond this count fail (power cut) */
static int failures = 0;

static uint32_t fake_read(uint32_t i) { return ee[i]; }
static int fake_write(uint32_t i, uint32_t v)
{
  if (fail_after >= 0 && writes >= fail_after) return -1;
  ee[i] = v;
  writes++;
  return 0;
}
static const nv_backend fake = { fake_read, fake_write };

static void check(int ok, const char *name)
{
  printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) failures++;
}

int main(void)
{
  int w;

  check(VER(1, 2, 0) > VER(1, 1, 65535) && VER(2, 0, 0) > VER(1, 255, 65535),
        "packed versions compare in the right order (major, then minor, then patch)");

  memset(ee, 0, sizeof ee);
  check(rollback_min_version(&fake) == 0, "erased EEPROM: minimum is 0 (anything boots)");

  check(rollback_raise(&fake, VER(1, 0, 0)) == 0 && rollback_min_version(&fake) == VER(1, 0, 0),
        "first boot of 1.0.0 raises the minimum to 1.0.0");
  check(rollback_raise(&fake, VER(1, 1, 0)) == 0 && rollback_min_version(&fake) == VER(1, 1, 0),
        "booting 1.1.0 raises the minimum to 1.1.0");

  w = writes;
  check(rollback_raise(&fake, VER(1, 0, 0)) == 0 && rollback_min_version(&fake) == VER(1, 1, 0) && writes == w,
        "an older version never lowers the minimum (and writes nothing)");
  check(rollback_raise(&fake, VER(1, 1, 0)) == 0 && writes == w,
        "the same version again writes nothing (no EEPROM wear on every boot)");

  /* Power cut during the first word of copy A while raising to 2.0.0 */
  fail_after = writes + 1;
  check(rollback_raise(&fake, VER(2, 0, 0)) == -1, "power cut mid-write is reported");
  fail_after = -1;
  check(rollback_min_version(&fake) == VER(1, 1, 0),
        "after a power cut in copy A: copy B still gives the old minimum, 1.1.0");

  /* Power cut after copy A finished, before copy B */
  fail_after = writes + 2;
  rollback_raise(&fake, VER(2, 0, 0));
  fail_after = -1;
  check(rollback_min_version(&fake) == VER(2, 0, 0),
        "power cut between the copies: the new minimum (2.0.0) already counts");

  check(ee[2] == VER(1, 1, 0), "  (copy B is still stale at 1.1.0 after that power cut)");
  rollback_raise(&fake, VER(2, 0, 0));          /* next boot of 2.0.0 */
  check(ee[2] == VER(2, 0, 0), "next boot repairs the stale copy B (even though the minimum did not change)");
  ee[0] ^= 0x00010000U;                         /* bit flip in copy A */
  check(rollback_min_version(&fake) == VER(2, 0, 0), "one corrupted copy: the other copy still holds 2.0.0");
  rollback_raise(&fake, VER(2, 0, 0));
  check(ee[0] == VER(2, 0, 0) && ee[1] == ~VER(2, 0, 0), "next boot repairs the corrupted copy A");

  ee[0] ^= 0x00010000U;                         /* damage both copies */
  ee[2] ^= 0x00010000U;
  check(rollback_min_version(&fake) == 0, "both copies corrupted: falls back to 0 (documented weakness)");

  printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL TESTS PASSED", failures);
  return failures ? 1 : 0;
}
