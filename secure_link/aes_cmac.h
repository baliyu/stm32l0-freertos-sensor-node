/* aes_cmac.h - AES-CMAC (RFC 4493 / NIST SP 800-38B), the MAC LoRaWAN uses for its MIC. */
#ifndef AES_CMAC_H
#define AES_CMAC_H

#include <stdint.h>
#include <stddef.h>
#include "aes128.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Full 16-byte tag over msg[0..len-1]. Truncate the tag yourself if you need fewer bytes. */
void aes_cmac(const aes128_ctx *ctx, const uint8_t *msg, size_t len, uint8_t tag[16]);

#ifdef __cplusplus
}
#endif

#endif
