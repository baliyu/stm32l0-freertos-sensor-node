/* bootloader/test/test_ob_check.c - PC tests for the stage 6 option-byte check.
 * Register values marked "board" were read from the real B-L072Z-LRWAN1.
 *
 *   gcc -Wall -Wextra -Werror -I.. -o test_ob_check test_ob_check.c ../ob_check.c && ./test_ob_check
 */
#include <stdio.h>
#include "ob_check.h"

static int failures = 0;

static void expect(const char *name, uint32_t optr, uint32_t wrp,
                   uint32_t want_flags, unsigned want_rdp,
                   int want_fatal_dev, int want_fatal_prod)
{
  uint32_t f   = ob_check(optr, wrp);
  unsigned rdp = ob_rdp_level(optr);
  int fd = ob_is_fatal(f, 0), fp = ob_is_fatal(f, 1);
  int ok = (f == want_flags) && (rdp == want_rdp) && (fd == want_fatal_dev) && (fp == want_fatal_prod);

  printf("[%s] %-46s OPTR=0x%08lX WRPROT1=0x%08lX flags=0x%lX rdp=%u fatal(dev/prod)=%d/%d\n",
         ok ? "PASS" : "FAIL", name, (unsigned long)optr, (unsigned long)wrp,
         (unsigned long)f, rdp, fd, fp);
  if (!ok) failures++;
}

int main(void)
{
  /*      name                                       OPTR        WRPROT1     flags                        rdp dev prod */
  expect("board before stage 6 (no WRP)",           0x807000AA, 0x00000000, OB_ERR_WRP | OB_WARN_RDP0,   0,  1,  1);
  expect("board after 6.2 (WRP 0-5, level 0)",      0x807000AA, 0x0000003F, OB_WARN_RDP0,                0,  0,  1);
  expect("production target (WRP 0-5, level 1)",    0x807000BB, 0x0000003F, OB_OK,                       1,  0,  0);
  expect("only sectors 0-4 protected",              0x807000BB, 0x0000001F, OB_ERR_WRP,                  1,  1,  1);
  expect("sector 0 missing",                        0x807000BB, 0x0000003E, OB_ERR_WRP,                  1,  1,  1);
  expect("extra sectors protected too (allowed)",   0x807000BB, 0x000000FF, OB_OK,                       1,  0,  0);
  expect("WPRMOD=1 (bits mean PCROP)",              0x807001BB, 0x0000003F, OB_ERR_WPRMOD,               1,  1,  1);
  expect("BFB2=1 (boot from bank 2)",               0x80F000BB, 0x0000003F, OB_ERR_BFB2,                 1,  1,  1);
  expect("everything wrong at level 0",             0x80F001AA, 0x00000000,
         OB_ERR_WRP | OB_ERR_WPRMOD | OB_ERR_BFB2 | OB_WARN_RDP0,                                         0,  1,  1);
  expect("RDP 0xCC = level 2",                      0x807000CC, 0x0000003F, OB_OK,                       2,  0,  0);
  expect("RDP 0x00 = level 1 (any other value)",    0x80700000, 0x0000003F, OB_OK,                       1,  0,  0);
  expect("RDP 0x55 = level 1 (any other value)",    0x80700055, 0x0000003F, OB_OK,                       1,  0,  0);
  expect("upper WRPROT bits ignored by mask",       0x807000BB, 0xFFFF003F, OB_OK,                       1,  0,  0);

  if (failures) { printf("%d TEST(S) FAILED\n", failures); return 1; }
  printf("ALL TESTS PASSED (ob_check)\n");
  return 0;
}
