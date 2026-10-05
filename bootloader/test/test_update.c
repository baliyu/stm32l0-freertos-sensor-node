/* test_update.c - slot B -> slot A updates on a simulated flash, including a
 * power cut at every few operations of the copy.
 *
 * Invariant checked after every cut: the board still has a complete, valid
 * image somewhere (slot A, or slot B waiting to be copied). And the next boot
 * always finishes the job. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "update.h"
#include "image.h"
#include "sha256.h"
#include "fixture_pubkey.h"

#define VER(a, b, c) (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | (uint32_t)(c))

#define FLASH_BASE_SIM SLOT_A_BASE
#define FLASH_SIZE_SIM (SLOT_B_BASE + SLOT_SIZE - SLOT_A_BASE)

static uint32_t flash_words[FLASH_SIZE_SIM / 4];
static uint8_t *flash = (uint8_t *)flash_words;
static long ops, cut_at = -1;
static int dead;
static int failures = 0;

static uint8_t img_old[IMG_HDR_SIZE + IMG_MAX_SIZE], img_new[sizeof img_old];
static size_t len_old, len_new;

static void check(int ok, const char *name)
{
  printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) failures++;
}

/* ---- simulated flash: erased = 0, a cut leaves the page/word in a bad state ---- */
static const uint8_t *sim_ptr(uint32_t addr) { return flash + (addr - FLASH_BASE_SIM); }

static int power_lost(void)
{
  if (dead) return 1;
  if (cut_at >= 0 && ops == cut_at) { dead = 1; return 1; }
  ops++;
  return 0;
}

static int sim_erase(uint32_t addr)
{
  uint8_t *p = flash + (addr - FLASH_BASE_SIM);
  if (power_lost()) { memset(p, 0xA5, FLASH_PAGE_SIZE); return -1; }   /* half-erased: garbage */
  memset(p, 0, FLASH_PAGE_SIZE);
  return 0;
}

static int sim_write(uint32_t addr, uint32_t v)
{
  uint8_t *p = flash + (addr - FLASH_BASE_SIM);
  uint32_t cur;
  memcpy(&cur, p, 4);
  if (power_lost()) { v = 0xDEADBEEF; memcpy(p, &v, 4); return -1; }   /* torn write */
  if (cur != 0) return -1;                                             /* like NOTZEROERR */
  memcpy(p, &v, 4);
  return 0;
}

static const flash_ops sim = { sim_ptr, sim_erase, sim_write, NULL };

/* ---- helpers ---- */
static size_t load(const char *path, uint8_t *dst)
{
  FILE *f = fopen(path, "rb");
  size_t n;
  if (!f) { printf("cannot open %s\n", path); exit(1); }
  n = fread(dst, 1, IMG_HDR_SIZE + IMG_MAX_SIZE, f);
  fclose(f);
  return n;
}

static void put(uint32_t slot, const uint8_t *img, size_t n)
{
  memset(flash + (slot - FLASH_BASE_SIM), 0, SLOT_SIZE);
  if (img) memcpy(flash + (slot - FLASH_BASE_SIM), img, n);
}

static void put_file(uint32_t slot, const char *path)
{
  static uint8_t tmp[IMG_HDR_SIZE + IMG_MAX_SIZE];
  size_t n = load(path, tmp);
  put(slot, tmp, n);
}

static int slot_valid(uint32_t slot)          /* full check, as the bootloader would run it from A */
{
  uint8_t d[32];
  return image_verify(sim_ptr(slot), SLOT_A_BASE, SIGNING_PUBKEY, d) == IMG_OK;
}

static int a_is(const uint8_t *img, size_t n) { return memcmp(sim_ptr(SLOT_A_BASE), img, n) == 0; }
static int b_empty(void) { return ((const img_header *)sim_ptr(SLOT_B_BASE))->magic != IMG_MAGIC; }

static upd_result run(uint32_t min, img_result *why)
{
  uint32_t v;
  ops = 0; dead = 0;
  return update_from_slot_b(&sim, SIGNING_PUBKEY, min, why, &v);
}

int main(void)
{
  img_result why;
  upd_result u;
  long total_ops, c, tested = 0, bad_invariant = 0, bad_finish = 0;

  len_old = load("fixture_good.bin", img_old);    /* v1.2.3, fake app 1 */
  len_new = load("fixture_new.bin", img_new);     /* v1.3.0, fake app 2 */

  /* Empty slot B */
  put(SLOT_A_BASE, img_old, len_old); put(SLOT_B_BASE, NULL, 0);
  u = run(0, &why);
  check(u == UPD_NONE && ops == 0 && a_is(img_old, len_old), "slot B empty: nothing happens, no flash writes");

  /* Good update */
  put(SLOT_A_BASE, img_old, len_old); put(SLOT_B_BASE, img_new, len_new);
  u = run(VER(1, 2, 3), &why);
  total_ops = ops;
  check(u == UPD_INSTALLED && a_is(img_new, len_new) && slot_valid(SLOT_A_BASE) && b_empty(),
        "good update: copied into A, A verifies, slot B cleared");
  u = run(VER(1, 3, 0), &why);
  check(u == UPD_NONE, "next boot: slot B empty, nothing to do");

  /* Bad updates: A must stay exactly as it was */
  put(SLOT_A_BASE, img_old, len_old); put_file(SLOT_B_BASE, "fixture_new_tampered.bin");
  u = run(0, &why);
  check(u == UPD_REJECTED && why == IMG_ERR_HASH && a_is(img_old, len_old) && b_empty(),
        "tampered update: rejected (hash), slot A untouched, slot B erased");

  put(SLOT_A_BASE, img_old, len_old); put_file(SLOT_B_BASE, "fixture_new_otherkey.bin");
  u = run(0, &why);
  check(u == UPD_REJECTED && why == IMG_ERR_SIGNATURE && a_is(img_old, len_old) && b_empty(),
        "update signed with another key: rejected (signature), slot A untouched");

  put(SLOT_A_BASE, img_old, len_old); put_file(SLOT_B_BASE, "fixture_new_rehashed.bin");
  u = run(0, &why);
  check(u == UPD_REJECTED && why == IMG_ERR_SIGNATURE && a_is(img_old, len_old),
        "update modified + re-hashed: rejected (signature), slot A untouched");

  put(SLOT_A_BASE, img_old, len_old); put(SLOT_B_BASE, img_new, len_new);
  u = run(VER(1, 4, 0), &why);
  check(u == UPD_REJECTED && why == IMG_ERR_ROLLBACK && a_is(img_old, len_old) && b_empty(),
        "update older than the minimum version: rejected (rollback), slot A untouched");

  /* Copy already done before a power cut cleared B: finish without rewriting A */
  put(SLOT_A_BASE, img_new, len_new); put(SLOT_B_BASE, img_new, len_new);
  u = run(0, &why);
  check(u == UPD_ALREADY && ops == 1 && b_empty(), "A already equals B: only slot B is cleared (1 operation)");

  /* Power cut at every 7th operation of a full install, then reboot */
  for (c = 0; c <= total_ops; c += (c < total_ops - 7 ? 7 : 1))
  {
    put(SLOT_A_BASE, img_old, len_old); put(SLOT_B_BASE, img_new, len_new);
    cut_at = c;
    run(VER(1, 2, 3), &why);                       /* power dies at operation c */
    cut_at = -1;
    tested++;

    /* Invariant: something valid is still there to boot or to install */
    if (!(slot_valid(SLOT_A_BASE) || slot_valid(SLOT_B_BASE))) bad_invariant++;

    /* Next boot (power back) must finish the update */
    u = run(VER(1, 2, 3), &why);
    if (!(a_is(img_new, len_new) && slot_valid(SLOT_A_BASE) && b_empty())) bad_finish++;
  }
  {
    char msg[160];
    snprintf(msg, sizeof msg, "power cut at %ld points of a %ld-operation install: always a valid image left", tested, total_ops);
    check(bad_invariant == 0, msg);
    snprintf(msg, sizeof msg, "power cut at %ld points: the next boot always completes the update", tested);
    check(bad_finish == 0, msg);
  }

  printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL TESTS PASSED", failures);
  return failures ? 1 : 0;
}
