/* image.h - secure-boot image format and slot layout (shared with tools/mkimage.py).
 *
 * Slot A (72 KB at 0x08006000):
 *   0x000  header (512 bytes)
 *   0x200  application: vector table, then code (linked at 0x08006200)
 *
 * Header (little-endian):
 *   off  size  field
 *   0    4     magic        IMG_MAGIC ("SBH1")
 *   4    4     hdr_version  1
 *   8    4     hdr_size     512
 *   12   4     img_size     application size in bytes
 *   16   4     fw_version   major<<24 | minor<<16 | patch
 *   20   32    sha256       SHA-256 over header bytes 0..19 followed by the application
 *   52   12    reserved     zero
 *   64   64    signature    ECDSA P-256 signature of sha256: r || s, 32 bytes each, big-endian
 *   128  384   reserved     zero
 *
 * The hash covers the fixed fields as well as the code, so the version and
 * size cannot be changed without the check failing. The signature covers the
 * hash, so nobody without the private key can produce an image that passes:
 * recomputing the hash after a change is no longer enough.
 */
#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h>

#define IMG_MAGIC        0x31484253UL      /* bytes 'S','B','H','1' in memory */
#define IMG_HDR_VERSION  1UL
#define IMG_HDR_SIZE     0x200UL
#define IMG_FIXED_LEN    20U               /* header bytes covered by the hash */

#define SLOT_A_BASE      0x08006000UL      /* active: the image that runs */
#define SLOT_B_BASE      0x08018000UL      /* update: a new image is placed here first */
#define SLOT_SIZE        0x12000UL         /* 72 KB each */
#define APP_BASE         (SLOT_A_BASE + IMG_HDR_SIZE)
#define IMG_MAX_SIZE     (SLOT_SIZE - IMG_HDR_SIZE)

#define RAM_START        0x20000000UL
#define RAM_END          (0x20000000UL + 20UL * 1024UL)

typedef struct {
  uint32_t magic;
  uint32_t hdr_version;
  uint32_t hdr_size;
  uint32_t img_size;
  uint32_t fw_version;
  uint8_t  sha256[32];
  uint8_t  reserved0[12];
  uint8_t  signature[64];
  uint8_t  reserved1[384];
} img_header;

typedef enum {
  IMG_OK = 0,
  IMG_ERR_MAGIC,          /* no header: empty slot or a raw (unwrapped) app */
  IMG_ERR_FIELDS,         /* header version/size fields out of range */
  IMG_ERR_HASH,           /* image or header modified/corrupted */
  IMG_ERR_SIGNATURE,      /* hash consistent, but not signed by the trusted key */
  IMG_ERR_VECTORS,        /* signed, but the app's vector table is implausible */
  IMG_ERR_ROLLBACK        /* genuine, but older than the minimum allowed version */
} img_result;

/* The checks, in the order the bootloader runs them. slot points at the
 * slot's first byte (on the target: (const uint8_t *)SLOT_A_BASE; in tests: a
 * RAM copy). */

/* Header fields, then SHA-256 over the fixed fields + app; digest receives the hash. */
img_result image_check_hash(const uint8_t *slot, uint8_t digest[32]);

/* ECDSA P-256 signature in the header over digest, with the trusted public key (X||Y). */
img_result image_check_signature(const uint8_t *slot, const uint8_t digest[32], const uint8_t pubkey[64]);

/* Plausible Cortex-M vector table; slot_addr is where the slot lives. */
img_result image_check_vectors(const uint8_t *slot, uint32_t slot_addr);

/* All of the above. */
img_result image_verify(const uint8_t *slot, uint32_t slot_addr, const uint8_t pubkey[64], uint8_t digest[32]);

const char *image_result_str(img_result r);

#endif
