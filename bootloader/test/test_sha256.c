/* test_sha256.c - SHA-256 against the FIPS 180-4 example vectors, plus a check
 * that feeding data in odd-sized pieces gives the same answer as one call
 * (the bootloader hashes the header and the app in two pieces). */
#include <stdio.h>
#include <string.h>
#include "sha256.h"

static int failures = 0;

static void check(int ok, const char *name)
{
  printf("[%s] %s\n", ok ? "PASS" : "FAIL", name);
  if (!ok) failures++;
}

static void hash(const void *d, size_t n, uint8_t out[32])
{
  sha256_ctx c;
  sha256_init(&c);
  sha256_update(&c, (const uint8_t *)d, n);
  sha256_final(&c, out);
}

static void hexify(const uint8_t *d, char *out)
{
  for (int i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", d[i]);
}

static void vec(const char *msg, const char *expect, const char *name)
{
  uint8_t d[32];
  char h[65];
  hash(msg, strlen(msg), d);
  hexify(d, h);
  check(strcmp(h, expect) == 0, name);
}

int main(int argc, char **argv)
{
  /* "hashme" mode: print the hash of stdin (used for the cross-check with Python) */
  if (argc > 1 && strcmp(argv[1], "hashme") == 0)
  {
    static uint8_t buf[1 << 20];
    size_t n = fread(buf, 1, sizeof buf, stdin);
    uint8_t d[32]; char h[65];
    hash(buf, n, d); hexify(d, h);
    printf("%s\n", h);
    return 0;
  }

  vec("abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
      "SHA-256 FIPS 180-4 example: \"abc\" (one block)");
  vec("", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      "SHA-256 empty message");
  vec("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
      "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
      "SHA-256 FIPS 180-4 example: 448-bit message (padding spills into a second block)");

  {
    static char a[1000000];
    uint8_t d[32]; char h[65];
    memset(a, 'a', sizeof a);
    hash(a, sizeof a, d); hexify(d, h);
    check(strcmp(h, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0,
          "SHA-256 one million 'a' (long message)");
  }

  {
    uint8_t data[1000], one[32], parts[32];
    sha256_ctx c;
    size_t pos = 0, step = 1;
    for (int i = 0; i < 1000; i++) data[i] = (uint8_t)(i * 7 + 3);
    hash(data, sizeof data, one);
    sha256_init(&c);
    while (pos < sizeof data)
    {
      size_t n = step; if (pos + n > sizeof data) n = sizeof data - pos;
      sha256_update(&c, data + pos, n);
      pos += n; step = step * 3 % 97 + 1;          /* irregular piece sizes */
    }
    sha256_final(&c, parts);
    check(memcmp(one, parts, 32) == 0, "SHA-256 in irregular pieces == one call");
  }

  printf("\n%s: %d failure(s)\n", failures ? "FAILED" : "ALL TESTS PASSED", failures);
  return failures ? 1 : 0;
}
