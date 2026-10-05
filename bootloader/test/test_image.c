/* test_image.c - does the bootloader's image check accept exactly what
 * tools/mkimage.py produces with the trusted key, and reject everything else?
 * Reads slot images made by the tool (see Makefile) into RAM and runs the same
 * checks the bootloader runs. */
#include <stdio.h>
#include <string.h>
#include "image.h"
#include "sha256.h"
#include "fixture_pubkey.h"          /* the "trusted" test key, made by gen_signing_key.py */

static int failures = 0;
static uint32_t slot_words[(IMG_HDR_SIZE + IMG_MAX_SIZE) / 4];   /* aligned like flash */
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

static img_result verify_slot(void)
{
  uint8_t d[32];
  return image_verify(slot, SLOT_A_BASE, SIGNING_PUBKEY, d);
}

static img_result verify_file(const char *path)
{
  memset(slot, 0, sizeof slot_words);
  if (!load(path, slot)) return (img_result)-1;
  return verify_slot();
}

static void reset_slot(void) { memset(slot, 0, sizeof slot_words); memcpy(slot, good, good_len); }

/* What an attacker without the private key can do: change the app, then
 * recompute and store the hash so the integrity check passes again. */
static void rehash(void)
{
  img_header *h = (img_header *)slot;
  sha256_ctx c;
  sha256_init(&c);
  sha256_update(&c, slot, IMG_FIXED_LEN);
  sha256_update(&c, slot + IMG_HDR_SIZE, h->img_size);
  sha256_final(&c, h->sha256);
}

int main(void)
{
  img_header *h = (img_header *)slot;
  uint8_t d[32];

  good_len = load("fixture_good.bin", good);
  if (!good_len) return 1;

  check(verify_file("fixture_good.bin") == IMG_OK, "signed with the trusted key: accepted");
  check(verify_file("fixture_unsigned.bin") == IMG_ERR_SIGNATURE, "unsigned image (no --key): rejected, signature");
  check(verify_file("fixture_otherkey.bin") == IMG_ERR_SIGNATURE, "signed with a different key: rejected, signature");
  check(verify_file("fixture_tampered.bin") == IMG_ERR_HASH, "bit flipped after signing: rejected, hash");
  check(verify_file("fixture_rehashed.bin") == IMG_ERR_SIGNATURE,
        "bit flipped + hash recomputed (mkimage --tamper-rehash): hash passes, signature catches it");

  reset_slot(); slot[IMG_HDR_SIZE + 0x2000] ^= 0x40; rehash();
  check(image_check_hash(slot, d) == IMG_OK && verify_slot() == IMG_ERR_SIGNATURE,
        "attacker edits code and fixes the hash in C: still rejected, signature");

  reset_slot(); h->fw_version = 0x02000000; rehash();
  check(verify_slot() == IMG_ERR_SIGNATURE, "attacker bumps the version and fixes the hash: rejected, signature");

  reset_slot(); h->signature[5] ^= 0x01;
  check(verify_slot() == IMG_ERR_SIGNATURE, "1 bit flipped in the stored signature: rejected");

  reset_slot(); h->img_size = IMG_MAX_SIZE + 1;
  check(verify_slot() == IMG_ERR_FIELDS, "image size larger than the slot: rejected before hashing");

  memset(slot, 0, sizeof slot_words);
  check(verify_slot() == IMG_ERR_MAGIC, "erased slot (all zeros, as L0 flash reads): no header");

  reset_slot(); memmove(slot, slot + IMG_HDR_SIZE, good_len - IMG_HDR_SIZE);
  check(verify_slot() == IMG_ERR_MAGIC, "raw app without a header (e.g. flashed from CubeIDE): no header");

  printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL TESTS PASSED", failures);
  return failures ? 1 : 0;
}
