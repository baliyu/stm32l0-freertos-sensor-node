/* test_host.c - run on the PC before any of this goes near a microcontroller.
 *   1. AES-128 against the FIPS-197 example
 *   2. AES-CMAC against the four RFC 4493 examples
 *   3. secure_link: round trip, tampering, wrong key, replay
 * Build:  make      Run:  ./test_host
 */
#include <stdio.h>
#include <string.h>
#include "aes128.h"
#include "aes_cmac.h"
#include "secure_link.h"

static int failures = 0;

static void check(int ok, const char *name)
{
  printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) failures++;
}

static int hex_to_bytes(const char *hex, uint8_t *out)
{
  int n = 0;
  unsigned v;
  while (hex[0] && hex[1] && sscanf(hex, "%2x", &v) == 1) { out[n++] = (uint8_t)v; hex += 2; }
  return n;
}

static void test_aes(void)
{
  uint8_t key[16], pt[16], expect[16], out[16];
  aes128_ctx ctx;
  hex_to_bytes("000102030405060708090a0b0c0d0e0f", key);
  hex_to_bytes("00112233445566778899aabbccddeeff", pt);
  hex_to_bytes("69c4e0d86a7b0430d8cdb78070b4c55a", expect);
  aes128_init(&ctx, key);
  aes128_encrypt(&ctx, pt, out);
  check(memcmp(out, expect, 16) == 0, "AES-128 FIPS-197 Appendix C.1");
}

static void test_cmac(void)
{
  static const char *msg_hex =
    "6bc1bee22e409f96e93d7e117393172a"
    "ae2d8a571e03ac9c9eb76fac45af8e51"
    "30c81c46a35ce411e5fbc1191a0a52ef"
    "f69f2445df4f9b17ad2b417be66c3710";
  static const struct { size_t len; const char *tag; const char *name; } v[4] = {
    {  0, "bb1d6929e95937287fa37d129b756746", "AES-CMAC RFC 4493 example 1 (0 bytes)"  },
    { 16, "070a16b46b4d4144f79bdd9dd04a287c", "AES-CMAC RFC 4493 example 2 (16 bytes)" },
    { 40, "dfa66747de9ae63030ca32611497c827", "AES-CMAC RFC 4493 example 3 (40 bytes)" },
    { 64, "51f0bebf7e3b9d92fc49741779363cfe", "AES-CMAC RFC 4493 example 4 (64 bytes)" },
  };
  uint8_t key[16], msg[64], expect[16], tag[16];
  aes128_ctx ctx;
  int i;

  hex_to_bytes("2b7e151628aed2a6abf7158809cf4f3c", key);
  hex_to_bytes(msg_hex, msg);
  aes128_init(&ctx, key);
  for (i = 0; i < 4; i++)
  {
    hex_to_bytes(v[i].tag, expect);
    aes_cmac(&ctx, msg, v[i].len, tag);
    check(memcmp(tag, expect, 16) == 0, v[i].name);
  }
}

static void test_link(void)
{
  /* Example keys for the test only. Real devices need their own random keys. */
  const uint8_t enc_key[16] = {0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17,0x18,0x19,0x1a,0x1b,0x1c,0x1d,0x1e,0x1f};
  const uint8_t mic_key[16] = {0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x2f};
  const uint8_t bad_key[16] = {0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,0x28,0x29,0x2a,0x2b,0x2c,0x2d,0x2e,0x30};
  const char *msg = "T=17.18";
  uint8_t len = (uint8_t)strlen(msg);
  uint8_t pkt[SL_MAX_PACKET], pkt2[SL_MAX_PACKET], bad[SL_MAX_PACKET], out[SL_MAX_PAYLOAD];
  uint8_t dev = 0;
  uint32_t last = 0;
  sl_keys tx, rx, wrong;
  int n, n2, r;

  sl_init_keys(&tx, enc_key, mic_key);
  sl_init_keys(&rx, enc_key, mic_key);
  sl_init_keys(&wrong, enc_key, bad_key);

  n = sl_seal(&tx, 0x42, 1, (const uint8_t *)msg, len, pkt, sizeof(pkt));
  check(n == (int)(SL_HDR_LEN + len + SL_MIC_LEN), "seal: packet length = header + payload + MIC");
  check(memcmp(&pkt[SL_HDR_LEN], msg, len) != 0, "seal: payload is not sent in plain text");

  /* A receiver with the wrong MIC key must reject it and leave its counter alone */
  r = sl_open(&wrong, &last, pkt, (uint8_t)n, &dev, out, sizeof(out));
  check(r == SL_ERR_MIC && last == 0, "open: wrong key rejected (MIC), counter unchanged");

  /* Genuine packet */
  r = sl_open(&rx, &last, pkt, (uint8_t)n, &dev, out, sizeof(out));
  check(r == len && memcmp(out, msg, len) == 0 && dev == 0x42 && last == 1,
        "open: genuine packet accepted and decrypted correctly");

  /* Same packet again: replay */
  r = sl_open(&rx, &last, pkt, (uint8_t)n, &dev, out, sizeof(out));
  check(r == SL_ERR_REPLAY, "open: replayed packet rejected");

  /* One flipped bit in the ciphertext */
  n2 = sl_seal(&tx, 0x42, 2, (const uint8_t *)msg, len, pkt2, sizeof(pkt2));
  memcpy(bad, pkt2, (size_t)n2);
  bad[SL_HDR_LEN] ^= 0x01;
  r = sl_open(&rx, &last, bad, (uint8_t)n2, &dev, out, sizeof(out));
  check(r == SL_ERR_MIC && last == 1, "open: 1-bit change in ciphertext rejected");

  /* Attacker bumps the counter to dodge replay protection */
  memcpy(bad, pkt, (size_t)n);
  bad[1] = 0x63;
  r = sl_open(&rx, &last, bad, (uint8_t)n, &dev, out, sizeof(out));
  check(r == SL_ERR_MIC && last == 1, "open: edited counter rejected (MIC covers the header)");

  /* Packet 2 is genuine and newer: accepted */
  r = sl_open(&rx, &last, pkt2, (uint8_t)n2, &dev, out, sizeof(out));
  check(r == len && last == 2, "open: next genuine packet (fcnt=2) accepted");

  /* Same plaintext, different counter -> different ciphertext */
  check(memcmp(&pkt[SL_HDR_LEN], &pkt2[SL_HDR_LEN], len) != 0,
        "seal: same reading encrypts differently under a new counter");

  /* Truncated packet */
  r = sl_open(&rx, &last, pkt2, 6, &dev, out, sizeof(out));
  check(r == SL_ERR_LENGTH, "open: truncated packet rejected");
}

int main(void)
{
  test_aes();
  test_cmac();
  test_link();
  printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL TESTS PASSED", failures);
  return failures ? 1 : 0;
}
