/* secure_link.c - see secure_link.h for the packet format. */
#include "secure_link.h"
#include "aes_cmac.h"
#include <string.h>

void sl_init_keys(sl_keys *k, const uint8_t enc_key[16], const uint8_t mic_key[16])
{
  aes128_init(&k->enc, enc_key);
  aes128_init(&k->mic, mic_key);
}

static void put_u32le(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static uint32_t get_u32le(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* XOR data with the AES-CTR keystream. The counter block is unique per
 * (dev_id, fcnt, block index), so a keystream is never reused as long as
 * fcnt never repeats under the same key. Encrypt and decrypt are the same. */
static void ctr_xor(const aes128_ctx *ctx, uint8_t dev_id, uint32_t fcnt,
                    const uint8_t *in, uint8_t *out, uint8_t len)
{
  uint8_t block[16], ks[16];
  uint8_t i, done = 0, blk = 1;

  while (done < len)
  {
    memset(block, 0, 16);
    block[0] = 0x01U;                   /* block type, as in LoRaWAN's A_i blocks */
    block[1] = dev_id;
    put_u32le(&block[2], fcnt);
    block[15] = blk++;
    aes128_encrypt(ctx, block, ks);
    for (i = 0; i < 16 && done < len; i++, done++) out[done] = (uint8_t)(in[done] ^ ks[i]);
  }
}

/* Compare without an early exit, so timing doesn't reveal how many bytes matched */
static int ct_equal(const uint8_t *a, const uint8_t *b, uint8_t n)
{
  uint8_t diff = 0, i;
  for (i = 0; i < n; i++) diff |= (uint8_t)(a[i] ^ b[i]);
  return diff == 0;
}

int sl_seal(const sl_keys *k, uint8_t dev_id, uint32_t fcnt,
            const uint8_t *payload, uint8_t len,
            uint8_t *out, uint8_t out_max)
{
  uint8_t tag[16];
  uint8_t total = (uint8_t)(SL_HDR_LEN + len + SL_MIC_LEN);

  if (len > SL_MAX_PAYLOAD || total > out_max) return SL_ERR_LENGTH;

  out[0] = dev_id;
  put_u32le(&out[1], fcnt);
  ctr_xor(&k->enc, dev_id, fcnt, payload, &out[SL_HDR_LEN], len);    /* encrypt ...  */
  aes_cmac(&k->mic, out, SL_HDR_LEN + len, tag);                      /* ... then MAC */
  memcpy(&out[SL_HDR_LEN + len], tag, SL_MIC_LEN);
  return total;
}

int sl_open(const sl_keys *k, uint32_t *last_fcnt,
            const uint8_t *pkt, uint8_t len,
            uint8_t *dev_id, uint8_t *payload, uint8_t payload_max)
{
  uint8_t tag[16];
  uint8_t plen;
  uint32_t fcnt;

  if (len < SL_HDR_LEN + SL_MIC_LEN || len > SL_MAX_PACKET) return SL_ERR_LENGTH;
  plen = (uint8_t)(len - SL_HDR_LEN - SL_MIC_LEN);
  if (plen > payload_max) return SL_ERR_LENGTH;

  /* 1. Authenticate first: nothing in the packet is trusted until the MIC matches */
  aes_cmac(&k->mic, pkt, SL_HDR_LEN + plen, tag);
  if (!ct_equal(tag, &pkt[SL_HDR_LEN + plen], SL_MIC_LEN)) return SL_ERR_MIC;

  /* 2. Then reject old or repeated counters */
  fcnt = get_u32le(&pkt[1]);
  if (fcnt <= *last_fcnt) return SL_ERR_REPLAY;

  /* 3. Only now decrypt, and only now move the counter forward */
  ctr_xor(&k->enc, pkt[0], fcnt, &pkt[SL_HDR_LEN], payload, plen);
  *dev_id = pkt[0];
  *last_fcnt = fcnt;
  return plen;
}
