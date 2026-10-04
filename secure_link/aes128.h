/* aes128.h - minimal AES-128 block encryption (encrypt direction only).
 * CTR mode and CMAC only ever use the forward cipher, so no decrypt code is needed. */
#ifndef AES128_H
#define AES128_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint8_t rk[176];          /* 11 round keys x 16 bytes */
} aes128_ctx;

void aes128_init(aes128_ctx *ctx, const uint8_t key[16]);
void aes128_encrypt(const aes128_ctx *ctx, const uint8_t in[16], uint8_t out[16]);

#ifdef __cplusplus
}
#endif

#endif
