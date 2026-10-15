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

/* Start a new hash. A context that has been finished must be initialised again before it is reused. */
void sha256_init(sha256_ctx *c);
/* Add len bytes of data; may be called any number of times, with the data split any way. */
void sha256_update(sha256_ctx *c, const uint8_t *data, size_t len);
/* Pad and finish: writes the 32-byte digest (big-endian words, as FIPS 180-4). */
void sha256_final(sha256_ctx *c, uint8_t digest[32]);

#ifdef __cplusplus
}
#endif

#endif
