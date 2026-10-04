/* test_fcnt.c - PC tests for fcnt_store using a fake EEPROM in RAM.
 * Simulates reboots and power loss in the middle of a storage write. */
#include <stdio.h>
#include <string.h>
#include "fcnt_store.h"

static uint32_t ee[FCNT_WORDS];          /* fake EEPROM; real L0 EEPROM erases to 0 */
static int writes = 0;
static int fail_after = -1;              /* >= 0: writes beyond this count fail (power cut) */
static int failures = 0;

static uint32_t fake_read(uint32_t i) { return ee[i]; }
static int fake_write(uint32_t i, uint32_t v)
{
  if (fail_after >= 0 && writes >= fail_after) return -1;   /* power gone: nothing written */
  ee[i] = v;
  writes++;
  return 0;
}
static const fcnt_backend fake = { fake_read, fake_write };

static void check(int ok, const char *name)
{
  printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) failures++;
}

int main(void)
{
  fcnt_store s;
  uint32_t c = 0, last = 0, i;
  int ok;

  /* 1. Fresh device: counters start at 1 and increase by 1 */
  memset(ee, 0, sizeof ee);
  ok = fcnt_store_init(&s, &fake) == 0;
  ok = ok && fcnt_store_next(&s, &c) == 0 && c == 1;
  ok = ok && fcnt_store_next(&s, &c) == 0 && c == 2;
  check(ok, "fresh storage: counter starts at 1 and counts up");

  /* 2. Use 250 values (crossing two block boundaries), then reboot */
  for (i = 0; i < 248; i++) fcnt_store_next(&s, &c);
  last = c;                                            /* 250 */
  fcnt_store_init(&s, &fake);
  fcnt_store_next(&s, &c);
  check(last == 250 && c > last, "reboot: first counter after reset is above every value used");

  /* 3. Reboot repeatedly without sending: still never goes backwards */
  last = c;
  ok = 1;
  for (i = 0; i < 20; i++)
  {
    fcnt_store_init(&s, &fake);
    fcnt_store_next(&s, &c);
    if (c <= last) ok = 0;
    last = c;
  }
  check(ok, "20 reboots in a row: counter never repeats or goes backwards");

  /* 4. Storage writes per counter stay low (wear) */
  memset(ee, 0, sizeof ee); writes = 0;
  fcnt_store_init(&s, &fake);
  for (i = 0; i < 1000; i++) fcnt_store_next(&s, &c);
  check(writes <= (int)(2U * (1000U / FCNT_BLOCK + 1U)), "1000 counters cost about 2 writes per 100 (wear limited)");

  /* 5. Power cut after the first word of a reservation write */
  memset(ee, 0, sizeof ee); writes = 0;
  fcnt_store_init(&s, &fake);                          /* reserves 101 (2 writes) */
  for (i = 0; i < 100; i++) fcnt_store_next(&s, &c);   /* uses 1..100 */
  last = c;                                            /* 100 */
  fail_after = writes + 1;                             /* next reservation: 1 word lands, then power dies */
  ok = (fcnt_store_next(&s, &c) != 0);                 /* must refuse to hand out 101 */
  fail_after = -1;
  check(ok, "power cut mid-write: no counter handed out beyond the stored limit");
  fcnt_store_init(&s, &fake);                          /* reboot: half-written slot is ignored */
  fcnt_store_next(&s, &c);
  check(c > last, "after power cut + reboot: resumes above every value used");

  /* 6. Newest slot corrupted later (bit flip): still no reuse */
  memset(ee, 0, sizeof ee);
  fcnt_store_init(&s, &fake);
  for (i = 0; i < 150; i++) fcnt_store_next(&s, &c);
  last = c;
  ee[s.newest * 2U] ^= 0x10U;                          /* damage the newest slot */
  fcnt_store_init(&s, &fake);
  fcnt_store_next(&s, &c);
  check(c > last, "corrupted newest slot: falls back safely, resumes above every value used");

  /* 7. Both slots erased-looking (all zero) reads as fresh, starts at 1 */
  memset(ee, 0, sizeof ee);
  fcnt_store_init(&s, &fake);
  fcnt_store_next(&s, &c);
  check(c == 1, "erased EEPROM (all zeros) is treated as fresh");

  printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL TESTS PASSED", failures);
  return failures ? 1 : 0;
}
