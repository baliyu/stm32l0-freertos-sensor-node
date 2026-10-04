/* image.c - image header and hash check, shared by the bootloader and PC tests. */
#include "image.h"
#include "sha256.h"
#include <string.h>

_Static_assert(sizeof(img_header) == IMG_HDR_SIZE, "header must be 512 bytes");

static uint32_t rd32(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

img_result image_verify(const uint8_t *slot, uint32_t slot_addr, uint8_t digest[32])
{
  const img_header *h = (const img_header *)slot;
  const uint8_t *app = slot + IMG_HDR_SIZE;
  uint32_t app_addr = slot_addr + IMG_HDR_SIZE;
  sha256_ctx c;
  uint32_t sp, reset;

  memset(digest, 0, 32);

  if (h->magic != IMG_MAGIC) return IMG_ERR_MAGIC;
  if (h->hdr_version != IMG_HDR_VERSION || h->hdr_size != IMG_HDR_SIZE ||
      h->img_size < 8U || h->img_size > IMG_MAX_SIZE)
    return IMG_ERR_FIELDS;

  /* Hash: fixed header fields, then the whole application */
  sha256_init(&c);
  sha256_update(&c, slot, IMG_FIXED_LEN);
  sha256_update(&c, app, h->img_size);
  sha256_final(&c, digest);
  if (memcmp(digest, h->sha256, 32) != 0) return IMG_ERR_HASH;

  /* Plausible Cortex-M vector table (now inside a hash-checked image) */
  sp = rd32(app);
  reset = rd32(app + 4);
  if (sp < RAM_START || sp > RAM_END || (sp & 3U) != 0) return IMG_ERR_VECTORS;
  if ((reset & 1U) == 0 || reset < app_addr || reset >= app_addr + h->img_size) return IMG_ERR_VECTORS;

  return IMG_OK;
}

const char *image_result_str(img_result r)
{
  switch (r)
  {
    case IMG_OK:          return "OK";
    case IMG_ERR_MAGIC:   return "no image header (empty slot or unwrapped app)";
    case IMG_ERR_FIELDS:  return "bad header fields";
    case IMG_ERR_HASH:    return "SHA-256 MISMATCH: image corrupted or modified";
    case IMG_ERR_VECTORS: return "bad vector table";
    default:              return "unknown";
  }
}
