/* update.c - see update.h */
#include "update.h"
#include <string.h>

static uint32_t rd32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Erase slot B's header page: B is then "empty" (no magic). */
static int clear_slot_b(const flash_ops *f)
{
  return f->erase_page(SLOT_B_BASE);
}

upd_result update_from_slot_b(const flash_ops *f, const uint8_t pubkey[64], uint32_t min_version,
                              img_result *why, uint32_t *version)
{
  const uint8_t *b = f->ptr(SLOT_B_BASE);
  const uint8_t *a = f->ptr(SLOT_A_BASE);
  const img_header *hb = (const img_header *)b;
  uint8_t digest[32];
  uint32_t total, off;
  img_result r;

  *why = IMG_OK;
  *version = 0;
  if (hb->magic != IMG_MAGIC) return UPD_NONE;
  *version = hb->fw_version;

  /* 1. Check B completely before touching A. The vector table is checked for
   *    slot A's address, because that is where this image will run. */
  r = image_check_hash(b, digest);
  if (r == IMG_OK) r = image_check_signature(b, digest, pubkey);
  if (r == IMG_OK) r = image_check_vectors(b, SLOT_A_BASE);
  if (r == IMG_OK && hb->fw_version < min_version) r = IMG_ERR_ROLLBACK;
  if (r != IMG_OK)
  {
    *why = r;
    return (clear_slot_b(f) == 0) ? UPD_REJECTED : UPD_FAILED;
  }

  total = IMG_HDR_SIZE + hb->img_size;

  /* Already installed? (power cut after the copy but before B was cleared) */
  if (memcmp(a, b, total) == 0)
    return (clear_slot_b(f) == 0) ? UPD_ALREADY : UPD_FAILED;

  /* 2. Copy page by page: erase one page of A, then program its words from B */
  for (off = 0; off < total; off += FLASH_PAGE_SIZE)
  {
    uint32_t w;
    if (f->erase_page(SLOT_A_BASE + off) != 0) return UPD_FAILED;
    for (w = 0; w < FLASH_PAGE_SIZE && off + w < total; w += 4U)
    {
      if (f->write_word(SLOT_A_BASE + off + w, rd32(b + off + w)) != 0) return UPD_FAILED;
    }
    if (f->progress) f->progress(off + FLASH_PAGE_SIZE < total ? off + FLASH_PAGE_SIZE : total, total);
  }

  /* 3. A must now equal B; only then give up the copy in B */
  if (memcmp(a, b, total) != 0) return UPD_FAILED;
  return (clear_slot_b(f) == 0) ? UPD_INSTALLED : UPD_FAILED;
}

const char *update_result_str(upd_result r)
{
  switch (r)
  {
    case UPD_NONE:      return "slot B empty";
    case UPD_INSTALLED: return "update installed";
    case UPD_ALREADY:   return "update was already installed (finished after a power cut)";
    case UPD_REJECTED:  return "update REJECTED, slot B erased, slot A untouched";
    case UPD_FAILED:    return "update copy FAILED (flash error), will retry next boot";
    default:            return "unknown";
  }
}
