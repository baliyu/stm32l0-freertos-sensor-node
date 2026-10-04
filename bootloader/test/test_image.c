/* test_image.c - does the bootloader's image check accept exactly what
 * tools/mkimage.py produces, and reject every kind of modification?
 * Reads slot images made by the tool (see Makefile) into RAM and runs the
 * same image_verify() the bootloader runs. */
#include <stdio.h>
#include <string.h>
#include "image.h"

static int failures = 0;
static uint32_t slot_words[IMG_HDR_SIZE / 4 + IMG_MAX_SIZE / 4];   /* aligned like flash */
static uint8_t *slot = (uint8_t *)slot_words;
static uint8_t good[sizeof slot_words];
static size_t good_len;

static void check(int ok, const char *name)
{
  printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) failures++;
}

static size_t load(const char *path, uint8_t *dst)
{
  FILE *f = fopen(path, "rb");
  size_t n;
  if (!f) { printf("cannot open %s\n", path); return 0; }
  n = fread(dst, 1, sizeof slot_words, f);
  fclose(f);
  return n;
}

static img_result verify_copy(void)
{
  uint8_t d[32];
  return image_verify(slot, SLOT_A_BASE, d);
}

static void reset_slot(void) { memset(slot, 0, sizeof slot_words); memcpy(slot, good, good_len); }

int main(void)
{
  img_header *h = (img_header *)slot;

  good_len = load("fixture_good.bin", good);
  if (!good_len) return 1;

  reset_slot();
  check(verify_copy() == IMG_OK, "image made by mkimage.py is accepted");

  memset(slot, 0, sizeof slot_words);
  load("fixture_tampered.bin", slot);
  check(verify_copy() == IMG_ERR_HASH, "mkimage --tamper (1 bit in the app) is rejected: hash");

  reset_slot(); slot[IMG_HDR_SIZE + good_len - IMG_HDR_SIZE - 1] ^= 0x80;
  check(verify_copy() == IMG_ERR_HASH, "last byte of the app changed: hash");

  reset_slot(); h->fw_version += 1;
  check(verify_copy() == IMG_ERR_HASH, "version number edited in the header: hash (fixed fields are hashed)");

  reset_slot(); h->img_size -= 4;
  check(verify_copy() == IMG_ERR_HASH, "image size edited in the header: hash");

  reset_slot(); h->img_size = IMG_MAX_SIZE + 1;
  check(verify_copy() == IMG_ERR_FIELDS, "image size larger than the slot: rejected before hashing");

  reset_slot(); h->hdr_version = 2;
  check(verify_copy() == IMG_ERR_FIELDS, "unknown header version: rejected");

  memset(slot, 0, sizeof slot_words);
  check(verify_copy() == IMG_ERR_MAGIC, "erased slot (all zeros, as L0 flash reads): no header");

  reset_slot(); memmove(slot, slot + IMG_HDR_SIZE, good_len - IMG_HDR_SIZE);
  check(verify_copy() == IMG_ERR_MAGIC, "raw app without a header (e.g. flashed from CubeIDE): no header");

  printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL TESTS PASSED", failures);
  return failures ? 1 : 0;
}
