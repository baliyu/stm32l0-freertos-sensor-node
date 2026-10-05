/* sha256.h - SHA-256 (FIPS 180-4), streaming interface, no dynamic memory. */
#ifndef SHA256_H
#define SHA256_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t state[8];
  uint64_t bitlen;
  uint8_t  buf[64];
  uint32_t buflen;
} sha256_ctx;

void sha256_init(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const uint8_t *data, size_t len);
void sha256_final(sha256_ctx *c, uint8_t digest[32]);

#ifdef __cplusplus
}
#endif

#endif
