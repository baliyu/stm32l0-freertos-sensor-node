/* aes_cmac.c - AES-CMAC per RFC 4493. */
#include "aes_cmac.h"
#include <string.h>

/* Left-shift a 16-byte block by one bit; if the top bit fell off, XOR in 0x87 */
static void dbl(const uint8_t in[16], uint8_t out[16])
{
  uint8_t carry = (uint8_t)(in[0] & 0x80U);
  int i;
  for (i = 0; i < 15; i++) out[i] = (uint8_t)((in[i] << 1) | (in[i + 1] >> 7));
  out[15] = (uint8_t)(in[15] << 1);
  if (carry) out[15] ^= 0x87U;
}

void aes_cmac(const aes128_ctx *ctx, const uint8_t *msg, size_t len, uint8_t tag[16])
{
  uint8_t L[16], K1[16], K2[16], X[16], last[16];
  const uint8_t zero[16] = {0};
  size_t n, i, j;
  int complete;

  /* Subkeys: L = AES(K, 0), K1 = dbl(L), K2 = dbl(K1) */
  aes128_encrypt(ctx, zero, L);
  dbl(L, K1);
  dbl(K1, K2);

  n = (len + 15U) / 16U;                         /* number of blocks */
  if (n == 0) { n = 1; complete = 0; }           /* empty message: one padded block */
  else        { complete = ((len % 16U) == 0); }

  /* Last block: XOR with K1 if full, otherwise pad (0x80 00..) and XOR with K2 */
  for (j = 0; j < 16; j++)
  {
    size_t idx = 16U * (n - 1U) + j;
    uint8_t b;
    if (complete)              b = msg[idx];
    else if (idx < len)        b = msg[idx];
    else if (idx == len)       b = 0x80U;
    else                       b = 0x00U;
    last[j] = (uint8_t)(b ^ (complete ? K1[j] : K2[j]));
  }

  /* CBC-MAC over all blocks with a zero IV */
  memset(X, 0, 16);
  for (i = 0; i + 1U < n; i++)
  {
    for (j = 0; j < 16; j++) X[j] ^= msg[16U * i + j];
    aes128_encrypt(ctx, X, X);
  }
  for (j = 0; j < 16; j++) X[j] ^= last[j];
  aes128_encrypt(ctx, X, tag);
}
