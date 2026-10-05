/* test_ecdsa.c - micro-ecc's P-256 verification against the RFC 6979 A.2.5
 * test vectors (also checked with Python's 'cryptography' library), plus
 * changes that must make verification fail. */
#include <stdio.h>
#include <string.h>
#include "uECC.h"
#include "sha256.h"

static int failures = 0;

static void check(int ok, const char *name)
{
  printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) failures++;
}

static void hex(const char *h, uint8_t *out, int n)
{
  for (int i = 0; i < n; i++) { unsigned v; sscanf(h + 2 * i, "%2x", &v); out[i] = (uint8_t)v; }
}

static void sha(const char *msg, uint8_t d[32])
{
  sha256_ctx c;
  sha256_init(&c);
  sha256_update(&c, (const uint8_t *)msg, strlen(msg));
  sha256_final(&c, d);
}

int main(void)
{
  uint8_t pub[64], sig_sample[64], sig_test[64], d[32], bad[64];
  const struct uECC_Curve_t *curve = uECC_secp256r1();

  /* RFC 6979 A.2.5: P-256 public key */
  hex("60FED4BA255A9D31C961EB74C6356D68C049B8923B61FA6CE669622E60F29FB6", pub, 32);
  hex("7903FE1008B8BC99A41AE9E95628BC64F2F1B20C2D7E9F5177A3C294D4462299", pub + 32, 32);
  /* SHA-256 signatures of "sample" and "test" (r || s) */
  hex("EFD48B2AACB6A8FD1140DD9CD45E81D69D2C877B56AAF991C34D0EA84EAF3716", sig_sample, 32);
  hex("F7CB1C942D657C41D436C7A1B6E29F65F3E900DBB9AFF4064DC4AB2F843ACDA8", sig_sample + 32, 32);
  hex("F1ABB023518351CD71D881567B1EA663ED3EFCF6C5132B354F28D3B0B7D38367", sig_test, 32);
  hex("019F4113742A2B14BD25926B49C649155F267E60D3814B4C0CC84250E46F0083", sig_test + 32, 32);

  check(uECC_valid_public_key(pub, curve), "RFC 6979 public key is a valid P-256 point");

  sha("sample", d);
  check(uECC_verify(pub, d, 32, sig_sample, curve) == 1, "RFC 6979 A.2.5 \"sample\" signature verifies");
  sha("test", d);
  check(uECC_verify(pub, d, 32, sig_test, curve) == 1, "RFC 6979 A.2.5 \"test\" signature verifies");

  sha("test", d);
  check(uECC_verify(pub, d, 32, sig_sample, curve) == 0, "signature of \"sample\" rejected for \"test\"");

  sha("sample", d);
  memcpy(bad, sig_sample, 64); bad[0] ^= 0x01;
  check(uECC_verify(pub, d, 32, bad, curve) == 0, "1 bit flipped in r: rejected");
  memcpy(bad, sig_sample, 64); bad[63] ^= 0x01;
  check(uECC_verify(pub, d, 32, bad, curve) == 0, "1 bit flipped in s: rejected");

  memset(bad, 0, 64);
  check(uECC_verify(pub, d, 32, bad, curve) == 0, "all-zero signature (unsigned image): rejected");

  memcpy(bad, pub, 64); bad[10] ^= 0x01;
  check(uECC_verify(bad, d, 32, sig_sample, curve) == 0, "different public key: rejected");

  printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL TESTS PASSED", failures);
  return failures ? 1 : 0;
}
