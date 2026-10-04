/* secure_link.h - encrypt-then-MAC packets for a LoRa sensor link.
 *
 * Packet layout:
 *   [dev_id 1B][fcnt 4B little-endian][ciphertext N B][MIC 4B]
 *
 * - Confidentiality: AES-128 in counter (CTR) mode with key enc_key.
 * - Integrity/authenticity: AES-CMAC with a separate key mic_key over
 *   dev_id || fcnt || ciphertext, truncated to 4 bytes (as LoRaWAN's MIC).
 * - Replay protection: the receiver only accepts a frame counter greater
 *   than the last one it accepted.
 */
#ifndef SECURE_LINK_H
#define SECURE_LINK_H

#include <stdint.h>
#include "aes128.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SL_HDR_LEN       5U
#define SL_MIC_LEN       4U
#define SL_MAX_PAYLOAD   48U
#define SL_MAX_PACKET    (SL_HDR_LEN + SL_MAX_PAYLOAD + SL_MIC_LEN)

/* Error codes returned by sl_open (negative) */
#define SL_ERR_LENGTH   (-1)   /* packet too short or too long */
#define SL_ERR_MIC      (-2)   /* tag mismatch: altered, or wrong key */
#define SL_ERR_REPLAY   (-3)   /* counter not newer than the last accepted one */

typedef struct {
  aes128_ctx enc;              /* expanded encryption key */
  aes128_ctx mic;              /* expanded MIC key */
} sl_keys;

void sl_init_keys(sl_keys *k, const uint8_t enc_key[16], const uint8_t mic_key[16]);

/* Build a packet. Returns its length, or SL_ERR_LENGTH if it doesn't fit. */
int sl_seal(const sl_keys *k, uint8_t dev_id, uint32_t fcnt,
            const uint8_t *payload, uint8_t len,
            uint8_t *out, uint8_t out_max);

/* Check and decrypt a packet. On success returns the payload length, writes
 * the payload and the sender id, and advances *last_fcnt. On failure returns
 * a negative SL_ERR_* code and changes nothing. */
int sl_open(const sl_keys *k, uint32_t *last_fcnt,
            const uint8_t *pkt, uint8_t len,
            uint8_t *dev_id, uint8_t *payload, uint8_t payload_max);

#ifdef __cplusplus
}
#endif

#endif
