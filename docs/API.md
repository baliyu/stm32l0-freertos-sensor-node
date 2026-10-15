# Interfaces

What other code, or a person, can depend on in this repository: the C interfaces of the bootloader and the secure link, the image format and memory map they share with the
signing tool, the command-line tools, and the host tests. The design and the security reasoning are in the [README](../README.md) and in
[THREAT_MODEL.md](THREAT_MODEL.md); they are not repeated here.

The tables below are **generated** from the code by `python3 tools/make_api_docs.py` (between the `api:` markers; do not edit them by hand), and
`tests/test_docs.py` fails if they fall behind the code. The same test checks that the facts which the C headers, the Python tools, the linker scripts, the README and the
other documents each repeat (addresses, sizes, field offsets, the power-cut test count) still agree.

Not covered here: the application in `Core/`, the CubeMX and HAL code, the FreeRTOS tasks, `bootloader/main.c` (the boot flow is in the README), the Feather receiver sketch,
and three data headers: `bootloader/pubkey.h` (the public signing key, generated), `secure_link/secure_link_keys.example.h` (a template) and `secure_link/secure_link_keys.h`
(the secret link keys, generated and never committed).

## 1. Image format and memory map

An application is wrapped in a 512-byte header by `tools/mkimage.py` and placed in slot A (to run) or slot B (to be installed). The bootloader runs the checks in this order
(`image_verify` does the first three; `bootloader/main.c` does them one by one, then compares the version with the stored minimum and refuses an older image):

1. `image_check_hash`: header fields, then SHA-256 over the fixed header bytes and the application.
2. `image_check_signature`: the ECDSA P-256 signature in the header, over that hash, with the trusted public key.
3. `image_check_vectors`: the application's vector table is plausible for where it will run.
4. Anti-rollback: `fw_version` against `rollback_min_version`; only after step 2 is `fw_version` trustworthy, because the signature covers the hash that covers it.

### 1.1 Image header

<!-- api:layout:start -->
The header is 512 bytes (`IMG_HDR_SIZE` = 512), little-endian, read from the compiled `img_header` structure. The SHA-256 covers header bytes 0 to 19 (`IMG_FIXED_LEN` = 20) followed by the application.

| Offset | Size | Field | Meaning (from `image.h`) |
|---|---|---|---|
| 0 | 4 | `magic` | IMG_MAGIC ("SBH1") |
| 4 | 4 | `hdr_version` | 1 |
| 8 | 4 | `hdr_size` | 512 |
| 12 | 4 | `img_size` | application size in bytes |
| 16 | 4 | `fw_version` | major<<24 \| minor<<16 \| patch |
| 20 | 32 | `sha256` | SHA-256 over header bytes 0..19 followed by the application |
| 52 | 12 | `reserved0` | zero |
| 64 | 64 | `signature` | ECDSA P-256 signature of sha256: r \|\| s, 32 bytes each, big-endian |
| 128 | 384 | `reserved1` | zero |

`IMG_MAGIC` = `0x31484253`, `IMG_HDR_VERSION` = 1.
<!-- api:layout:end -->

### 1.2 Memory map

<!-- api:memory:start -->
| Region | Start | Last byte | Size | Defined in |
|---|---|---|---|---|
| Bootloader | `0x08000000` | `0x08005FFF` | 24 KB | `bootloader/bootloader.ld` |
| Slot A (header, then application) | `0x08006000` | `0x08017FFF` | 72 KB | `bootloader/image.h` |
| &nbsp;&nbsp;header | `0x08006000` | `0x080061FF` | 512 B | `bootloader/image.h` |
| &nbsp;&nbsp;application | `0x08006200` | `0x08017FFF` | 73,216 B | `bootloader/image.h`; `STM32L072xx_FLASH.ld` (FLASH) |
| Slot B (update) | `0x08018000` | `0x08029FFF` | 72 KB | `bootloader/image.h` |
| Data EEPROM: anti-rollback record | `0x08080100` | `0x0808010F` | 16 B | `bootloader/nv_eeprom.h`, `bootloader/rollback.h` |
| RAM | `0x20000000` | `0x20004FFF` | 20 KB | `bootloader/bootloader.ld` |

The application is linked at `0x08006200` for 73,216 B: its first byte is the first byte after the header.
<!-- api:memory:end -->

The same constants exist in `bootloader/image.h`, `tools/mkimage.py`, the linker scripts and the README's flash table; `tests/test_docs.py` compares them.

## 2. C interfaces

Conventions, as the headers define them:

- Check results are enums (`img_result`, `upd_result`) with a `*_result_str()` function for the serial log. `sl_open` returns the payload length or a negative `SL_ERR_*` code.
- Flash and non-volatile storage are reached through small structures of function pointers (`flash_ops`, `nv_backend`, `fcnt_backend`), so the same logic runs on the STM32 and in the
  PC tests. Their functions return 0 on success.
- `rollback_raise` and the `fcnt_store_*` functions return 0 on success and -1 when a storage write failed.

<!-- api:c:start -->
#### `bootloader/image.h`

secure-boot image format and slot layout (shared with tools/mkimage.py).

| Name | Declaration | What it does |
|---|---|---|
| `image_check_hash` | `img_result image_check_hash(const uint8_t *slot, uint8_t digest[32])` | Header fields, then SHA-256 over the fixed fields + app; digest receives the hash. |
| `image_check_signature` | `img_result image_check_signature(const uint8_t *slot, const uint8_t digest[32], const uint8_t pubkey[64])` | ECDSA P-256 signature in the header over digest, with the trusted public key (X\|\|Y). |
| `image_check_vectors` | `img_result image_check_vectors(const uint8_t *slot, uint32_t slot_addr)` | Plausible Cortex-M vector table; slot_addr is where the slot lives. |
| `image_verify` | `img_result image_verify(const uint8_t *slot, uint32_t slot_addr, const uint8_t pubkey[64], uint8_t digest[32])` | All of the above. |
| `image_result_str` | `const char *image_result_str(img_result r)` | Short text for an image result code, for the serial log. Returns a constant string. |

| Constant | Value | Meaning |
|---|---|---|
| `IMG_MAGIC` | `0x31484253UL` | bytes 'S','B','H','1' in memory |
| `IMG_HDR_VERSION` | `1UL` |  |
| `IMG_HDR_SIZE` | `0x200UL` |  |
| `IMG_FIXED_LEN` | `20U` | header bytes covered by the hash |
| `SLOT_A_BASE` | `0x08006000UL` | active: the image that runs |
| `SLOT_B_BASE` | `0x08018000UL` | update: a new image is placed here first |
| `SLOT_SIZE` | `0x12000UL` | 72 KB each |
| `APP_BASE` | `(SLOT_A_BASE + IMG_HDR_SIZE)` |  |
| `IMG_MAX_SIZE` | `(SLOT_SIZE - IMG_HDR_SIZE)` |  |
| `RAM_START` | `0x20000000UL` |  |
| `RAM_END` | `(0x20000000UL + 20UL * 1024UL)` |  |

```c
typedef struct {
  uint32_t magic;
  uint32_t hdr_version;
  uint32_t hdr_size;
  uint32_t img_size;
  uint32_t fw_version;
  uint8_t  sha256[32];
  uint8_t  reserved0[12];
  uint8_t  signature[64];
  uint8_t  reserved1[384];
} img_header;
```

```c
typedef enum {
  IMG_OK = 0,
  IMG_ERR_MAGIC,          /* no header: empty slot or a raw (unwrapped) app */
  IMG_ERR_FIELDS,         /* header version/size fields out of range */
  IMG_ERR_HASH,           /* image or header modified/corrupted */
  IMG_ERR_SIGNATURE,      /* hash consistent, but not signed by the trusted key */
  IMG_ERR_VECTORS,        /* signed, but the app's vector table is implausible */
  IMG_ERR_ROLLBACK        /* genuine, but older than the minimum allowed version */
} img_result;
```

#### `bootloader/update.h`

install a new image from slot B into slot A, safely.

| Name | Declaration | What it does |
|---|---|---|
| `update_from_slot_b` | `upd_result update_from_slot_b(const flash_ops *f, const uint8_t pubkey[64], uint32_t min_version, img_result *why, uint32_t *version)` | Install slot B into slot A if slot B holds a valid, newer-than-minimum image. min_version is the lowest fw_version still accepted (see rollback_min_version). *why receives the reason a slot B image was refused (IMG_OK otherwise); *version receives slot B's fw_version when it has a header (0 when it is empty). |
| `update_result_str` | `const char *update_result_str(upd_result r)` | Short text for an update result code, for the serial log. Returns a constant string. |

| Constant | Value | Meaning |
|---|---|---|
| `FLASH_PAGE_SIZE` | `128U` | STM32L0 program memory erase unit |

```c
typedef struct {
  const uint8_t *(*ptr)(uint32_t addr);               /* read access to flash at addr */
  int  (*erase_page)(uint32_t addr);                   /* erase the 128-byte page at addr; 0 = ok */
  int  (*write_word)(uint32_t addr, uint32_t value);   /* program one erased word; 0 = ok */
  void (*progress)(uint32_t done, uint32_t total);     /* optional, may be NULL */
} flash_ops;
```

```c
typedef enum {
  UPD_NONE = 0,       /* slot B empty: nothing to do */
  UPD_INSTALLED,      /* copied, checked and slot B cleared */
  UPD_ALREADY,        /* slot A already held this image (earlier power cut after the copy) */
  UPD_REJECTED,       /* slot B image refused (see why) and erased; slot A untouched */
  UPD_FAILED          /* flash error during the copy: slot B kept, retried next boot */
} upd_result;
```

#### `bootloader/rollback.h`

anti-rollback: the lowest firmware version the bootloader will still start, kept in non-volatile memory and only ever raised.

| Name | Declaration | What it does |
|---|---|---|
| `rollback_min_version` | `uint32_t rollback_min_version(const nv_backend *be)` | Current minimum version (0 if nothing valid has been stored yet). |
| `rollback_raise` | `int rollback_raise(const nv_backend *be, uint32_t version)` | Make both copies hold max(current minimum, version). Writes nothing when they already do, so it is safe (and wear-free) to call on every boot; when one copy is stale or damaged it is repaired. Returns 0 on success, -1 if a storage write failed. |

| Constant | Value | Meaning |
|---|---|---|
| `ROLLBACK_WORDS` | `4U` | storage needed: 2 copies x 2 words |

```c
typedef struct {
  uint32_t (*read)(uint32_t index);                 /* read word 0..3 */
  int      (*write)(uint32_t index, uint32_t value); /* 0 = ok */
} nv_backend;
```

#### `bootloader/ob_check.h`

secure boot, stage 6: boot-time check of the chip's protection settings (option bytes).

| Name | Declaration | What it does |
|---|---|---|
| `ob_rdp_level` | `unsigned ob_rdp_level(uint32_t optr)` | 0, 1 or 2 |
| `ob_check` | `uint32_t ob_check(uint32_t optr, uint32_t wrprot1)` | Returns a combination of the OB_ERR_* / OB_WARN_* flags, OB_OK if all is well. Extra protected sectors beyond 0-5 are allowed. |
| `ob_is_fatal` | `int ob_is_fatal(uint32_t flags, int require_rdp1)` | Non-zero if the bootloader must refuse to continue. require_rdp1 = 0 during development (level 0 only warns), = 1 for production builds (level 0 is fatal). |

| Constant | Value | Meaning |
|---|---|---|
| `OB_FLASH_OPTR_ADDR` | `0x4002201CUL` |  |
| `OB_FLASH_WRPROT1_ADDR` | `0x40022020UL` |  |
| `OB_BOOTLOADER_WRP_MASK` | `0x0000003FUL` |  |
| `OB_OPTR_RDP_MASK` | `0x000000FFUL` | AA = level 0, CC = level 2, else level 1 |
| `OB_OPTR_WPRMOD` | `(1UL << 8)` | 1 turns the WRPROT bits into PCROP |
| `OB_OPTR_BFB2` | `(1UL << 23)` | 1 = boot from bank 2 (= slot B area) |
| `OB_RDP_LEVEL0_VALUE` | `0xAAU` |  |
| `OB_RDP_LEVEL2_VALUE` | `0xCCU` |  |
| `OB_OK` | `0x0U` |  |
| `OB_ERR_WRP` | `(1U << 0)` | one or more bootloader sectors not write-protected |
| `OB_ERR_WPRMOD` | `(1U << 1)` | WRPROT bits mean PCROP, not write protection |
| `OB_ERR_BFB2` | `(1U << 2)` | boot path can skip this bootloader |
| `OB_WARN_RDP0` | `(1U << 3)` | read-out protection off (debugger has full access) |

#### `bootloader/sha256.h`

SHA-256 (FIPS 180-4), streaming interface, no dynamic memory.

| Name | Declaration | What it does |
|---|---|---|
| `sha256_init` | `void sha256_init(sha256_ctx *c)` | Start a new hash. A context that has been finished must be initialised again before it is reused. |
| `sha256_update` | `void sha256_update(sha256_ctx *c, const uint8_t *data, size_t len)` | Add len bytes of data; may be called any number of times, with the data split any way. |
| `sha256_final` | `void sha256_final(sha256_ctx *c, uint8_t digest[32])` | Pad and finish: writes the 32-byte digest (big-endian words, as FIPS 180-4). |

```c
typedef struct {
  uint32_t state[8];
  uint64_t bitlen;
  uint8_t  buf[64];
  uint32_t buflen;
} sha256_ctx;
```

#### `bootloader/flash_l0.h`

STM32L0 program-memory erase/program for the bootloader (no HAL).

| Name | Declaration | What it does |
|---|---|---|
| `flash_l0` | `extern const flash_ops flash_l0` | The STM32L0 implementation of flash_ops, used by the bootloader on the chip. |
| `flash_l0_progress` | `extern void (*flash_l0_progress)(uint32_t done, uint32_t total)` | Set by main.c to print copy progress (may stay NULL) |

#### `bootloader/nv_eeprom.h`

data EEPROM backend for the bootloader (register level, no HAL).

| Name | Declaration | What it does |
|---|---|---|
| `rollback_eeprom` | `extern const nv_backend rollback_eeprom` | The STM32L0 data-EEPROM implementation of nv_backend, holding the anti-rollback minimum. |

| Constant | Value | Meaning |
|---|---|---|
| `ROLLBACK_EEPROM_ADDR` | `0x08080100UL` |  |

#### `secure_link/secure_link.h`

encrypt-then-MAC packets for a LoRa sensor link.

| Name | Declaration | What it does |
|---|---|---|
| `sl_init_keys` | `void sl_init_keys(sl_keys *k, const uint8_t enc_key[16], const uint8_t mic_key[16])` | Expand the encryption key and the MIC key once; the same sl_keys then serves every packet. |
| `sl_seal` | `int sl_seal(const sl_keys *k, uint8_t dev_id, uint32_t fcnt, const uint8_t *payload, uint8_t len, uint8_t *out, uint8_t out_max)` | Build a packet. Returns its length, or SL_ERR_LENGTH if it doesn't fit. |
| `sl_open` | `int sl_open(const sl_keys *k, uint32_t *last_fcnt, const uint8_t *pkt, uint8_t len, uint8_t *dev_id, uint8_t *payload, uint8_t payload_max)` | Check and decrypt a packet. On success returns the payload length, writes the payload and the sender id, and advances *last_fcnt. On failure returns a negative SL_ERR_* code and changes nothing. |

| Constant | Value | Meaning |
|---|---|---|
| `SL_HDR_LEN` | `5U` |  |
| `SL_MIC_LEN` | `4U` |  |
| `SL_MAX_PAYLOAD` | `48U` |  |
| `SL_MAX_PACKET` | `(SL_HDR_LEN + SL_MAX_PAYLOAD + SL_MIC_LEN)` |  |
| `SL_ERR_LENGTH` | `(-1)` | packet too short or too long |
| `SL_ERR_MIC` | `(-2)` | tag mismatch: altered, or wrong key |
| `SL_ERR_REPLAY` | `(-3)` | counter not newer than the last accepted one |

```c
typedef struct {
  aes128_ctx enc;              /* expanded encryption key */
  aes128_ctx mic;              /* expanded MIC key */
} sl_keys;
```

#### `secure_link/fcnt_store.h`

frame counter that survives resets, without reusing a value.

| Name | Declaration | What it does |
|---|---|---|
| `fcnt_store_init` | `int fcnt_store_init(fcnt_store *s, const fcnt_backend *be)` | Load from storage and make the first reservation. 0 = ok, -1 = storage error. |
| `fcnt_store_next` | `int fcnt_store_next(fcnt_store *s, uint32_t *out)` | Hand out the next counter. 0 = ok; -1 = storage error (do NOT transmit). |

| Constant | Value | Meaning |
|---|---|---|
| `FCNT_BLOCK` | `100U` | counters reserved per storage write |
| `FCNT_WORDS` | `4U` | storage needed: 2 slots x 2 words |

```c
typedef struct {
  uint32_t (*read)(uint32_t index);                 /* read word 0..3 */
  int      (*write)(uint32_t index, uint32_t value); /* 0 = ok */
} fcnt_backend;
```

```c
typedef struct {
  const fcnt_backend *be;
  uint32_t next;                 /* next counter value to hand out */
  uint32_t reserved;             /* every value < reserved is covered by storage */
  uint8_t  newest;               /* slot holding the current reservation */
} fcnt_store;
```

#### `secure_link/aes128.h`

minimal AES-128 block encryption (encrypt direction only).

| Name | Declaration | What it does |
|---|---|---|
| `aes128_init` | `void aes128_init(aes128_ctx *ctx, const uint8_t key[16])` | Expand the 16-byte key into the 176-byte round-key schedule. |
| `aes128_encrypt` | `void aes128_encrypt(const aes128_ctx *ctx, const uint8_t in[16], uint8_t out[16])` | Encrypt one 16-byte block with an expanded key. |

```c
typedef struct {
  uint8_t rk[176];          /* 11 round keys x 16 bytes */
} aes128_ctx;
```

#### `secure_link/aes_cmac.h`

AES-CMAC (RFC 4493 / NIST SP 800-38B), the MAC LoRaWAN uses for its MIC.

| Name | Declaration | What it does |
|---|---|---|
| `aes_cmac` | `void aes_cmac(const aes128_ctx *ctx, const uint8_t *msg, size_t len, uint8_t tag[16])` | Full 16-byte tag over msg[0..len-1]. Truncate the tag yourself if you need fewer bytes. |
<!-- api:c:end -->

### 2.1 Examples

These are compiled by `tests/test_docs.py` (the secure-link one is also run). They show how the calls fit together; they are not the bootloader's own code.

<!-- examples:start -->
Check a slot (on the chip the slot is read in place from flash):

```c
#include "image.h"
#include "pubkey.h"

int slot_a_is_genuine(void)
{
  uint8_t digest[32];
  img_result r = image_verify((const uint8_t *)SLOT_A_BASE, SLOT_A_BASE, SIGNING_PUBKEY, digest);
  return r == IMG_OK;
}
```

Install an update waiting in slot B, with the anti-rollback minimum from the data EEPROM:

```c
#include "update.h"
#include "flash_l0.h"
#include "nv_eeprom.h"
#include "pubkey.h"

upd_result install_waiting_update(img_result *why, uint32_t *version)
{
  uint32_t min = rollback_min_version(&rollback_eeprom);
  return update_from_slot_b(&flash_l0, SIGNING_PUBKEY, min, why, version);
}
```

Seal a reading and open it again (the receiver tracks the last accepted frame counter):

```c
#include "secure_link.h"

int round_trip(const uint8_t enc[16], const uint8_t mic[16])
{
  sl_keys k;
  uint8_t pkt[SL_MAX_PACKET];
  uint8_t out[SL_MAX_PAYLOAD];
  uint8_t id;
  uint32_t last = 0;
  const uint8_t reading[2] = { 0x12, 0x34 };
  int n;

  sl_init_keys(&k, enc, mic);
  n = sl_seal(&k, 1, 1, reading, sizeof reading, pkt, sizeof pkt);
  if (n < 0) return n;
  return sl_open(&k, &last, pkt, (uint8_t)n, &id, out, sizeof out);   /* 2 = the payload length */
}
```
<!-- examples:end -->

## 3. Tools

Run them from the repository root.

<!-- api:tools:start -->
#### `tools/gen_signing_key.py`

create the firmware signing key pair.

| Option | What it does | Notes |
|---|---|---|
| `--key` | where the private key (PEM) is written | default `keys/signing_key.pem` |
| `--header` | where the public key is written as a C header | default `bootloader/pubkey.h` |
| `--force` | replace an existing private key (DANGEROUS) |  |

#### `tools/make_api_docs.py`

write the generated parts of docs/API.md from the code.

| Option | What it does | Notes |
|---|---|---|
| `--check` | do not write; exit 1 if docs/API.md is out of date |  |

#### `tools/make_eeprom_reset.py`

DEVELOPMENT ONLY: a .hex that clears the bootloader's anti-rollback record in data EEPROM (16 zero bytes at 0x08080100), so older test versions can boot again. Usage: python3 tools/make_eeprom_reset.py [out.hex] (default images/reset_min_version.hex).

#### `tools/make_image.sh`

turn a built app ELF into a signed slot A image.

```text
make_image.sh - turn a built app ELF into a signed slot A image.
  sh tools/make_image.sh <app.elf> <version> [out-name]
Example:
  sh tools/make_image.sh build/Debug/l072_blinky.elf 1.0.0
Signs with keys/signing_key.pem (create it with tools/gen_signing_key.py).
Set SLOT=B to make an update image for slot B instead of slot A:
  SLOT=B sh tools/make_image.sh build/Debug/l072_blinky.elf 1.2.0 images/update
```

#### `tools/mkimage.py`

wrap an application binary in the secure-boot image header.

| Option | What it does | Notes |
|---|---|---|
| `app_bin` | application binary linked at 0x08006200 |  |
| `--version` | firmware version, e.g. 1.0.0 | required |
| `-o`, `--out` | output name without extension (default: slotA) | default `slotA` |
| `--key` | private key (PEM) to sign with, e.g. keys/signing_key.pem |  |
| `--slot` | A: run directly (0x08006000); B: install as an update (0x08018000) | default `A`; one of `A`, `B` |
| `--tamper` | TEST ONLY: flip one bit at this offset in the app AFTER hashing |  |
| `--tamper-rehash` | TEST ONLY: flip one bit in the app and RECOMPUTE the hash, keeping the old signature (what an attacker without the private key could do) |  |

#### `tools/rdp_level1.sh`

secure boot stage 6: set read-out protection to Level 1 (RDP = 0xBB) on the B-L072Z-LRWAN1, and nothing else.

```text
tools/rdp_level1.sh - secure boot stage 6: set read-out protection to Level 1
(RDP = 0xBB) on the B-L072Z-LRWAN1, and nothing else.

  sh tools/rdp_level1.sh --check   read and check only, changes nothing
  sh tools/rdp_level1.sh           check, ask for confirmation, then set Level 1

Guard rails:
  - The only RDP value this script can ever write is 0xBB. Level 2 (0xCC) is
    permanent (the debug port is disabled for good) and is never used here.
  - It refuses to run unless the chip is at Level 0 AND the bootloader
    sectors 0-5 are write-protected (WRPROT1 bits 0-5), WPRMOD=0 and BFB2=0,
    i.e. exactly the state the stage 6 bootloader accepts.
  - It reads the result back and stops loudly if it is not Level 1.
Going back from Level 1 to Level 0 erases the whole flash and data EEPROM.
```

#### `secure_link/gen_keys.py`

Write secure_link_keys.h with two fresh random 128-bit keys.

#### `secure_link/sync_to_feather.sh`

Copy the shared crypto code and your keys into the Arduino sketch folder.

```text
Copy the shared crypto code and your keys into the Arduino sketch folder.
Arduino only compiles files that sit inside the sketch folder, so the Feather
gets copies; secure_link/ stays the single source of truth (copies are git-ignored).
```
<!-- api:tools:end -->

## 4. Tests

The C tests run on the PC with `gcc`; the image tests need Python's `cryptography` package, which the signing tool needs anyway.

```text
pip install cryptography
make -C bootloader/test test
make -C secure_link/test test
python3 -m unittest discover -s tests -v
```

<!-- api:tests:start -->
| Program | What it checks | Built and run by |
|---|---|---|
| `bootloader/test/test_ecdsa.c` | micro-ecc's P-256 verification against the RFC 6979 A.2.5 test vectors (also checked with Python's 'cryptography' library), plus changes that must make verification fail. | `make -C bootloader/test test` |
| `bootloader/test/test_image.c` | does the bootloader's image check accept exactly what tools/mkimage.py produces with the trusted key, and reject everything else? | `make -C bootloader/test test` |
| `bootloader/test/test_ob_check.c` | PC tests for the stage 6 option-byte check. | `make -C bootloader/test test` |
| `bootloader/test/test_rollback.c` | the anti-rollback store, with a fake EEPROM in RAM. | `make -C bootloader/test test` |
| `bootloader/test/test_sha256.c` | SHA-256 against the FIPS 180-4 example vectors, plus a check that feeding data in odd-sized pieces gives the same answer as one call (the bootloader hashes the header and the app in two pieces). | `make -C bootloader/test test` |
| `bootloader/test/test_update.c` | slot B -> slot A updates on a simulated flash, including a power cut at every few operations of the copy. | `make -C bootloader/test test` |
| `secure_link/test/test_fcnt.c` | PC tests for fcnt_store using a fake EEPROM in RAM. | `make -C secure_link/test test` |
| `secure_link/test/test_host.c` | AES-128, AES-CMAC and the secure link, run on the PC before any of this goes near a microcontroller. | `make -C secure_link/test test` |
<!-- api:tests:end -->

What `tests/test_docs.py` checks (this list is generated from the test file):

<!-- api:doctests:start -->
- tools/mkimage.py and bootloader/image.h agree on the magic, header size, slot addresses and sizes, and the RAM range.
- tools/make_eeprom_reset.py clears the address that bootloader/nv_eeprom.h says holds the anti-rollback record.
- The hand-written field table in image.h's comment gives the same offsets, sizes and names as the compiled img_header structure.
- An image built by tools/mkimage.py with a throw-away key has every field where the compiled structure says, and its signature verifies in Python.
- The bootloader's linker script ends where slot A starts; both application linker scripts start at the application and fit the slot; RAM is 20 KB in all three.
- Slot B starts where slot A ends, every slot boundary is a whole 128-byte page, and the write-protect mask covers exactly the bootloader's 4 KB sectors.
- The README's flash-layout table (bootloader, slot A, slot B, reserve) gives the same start addresses and sizes as image.h and the linker script, and adds up to the stated total.
- The tables in docs/API.md are exactly what tools/make_api_docs.py writes from the code today.
- Every .h file in bootloader/ and secure_link/ is either documented in API.md or listed as left out, with a reason.
- Every function and extern declaration in a documented header has a comment directly above it or on its line, and every header has a one-line summary.
- A second, simpler reading of each header finds the same functions, extern declarations and #define constants as appear in the generated tables.
- Every script in tools/, the two in secure_link/, and every test_*.c program has an entry in API.md.
- Every file or folder named in backticks in the README, SECURITY.md and docs/ exists in the repository (build outputs, the private key and the generated key headers excepted).
- The 'power cut at N points' figure in the README, the CRA mapping, the IEC 62443 assessment and the threat model equals what the update test reports when it runs.
- Every command-line option of the Python tools has help text, so the generated tool reference is never a row of empty descriptions.
- Each C example in API.md compiles with warnings as errors against the real headers, and the secure-link example round-trips a packet when linked with the real code.
- Every test_*.c has a build rule in its Makefile, is a prerequisite of the default test target, and is run by it: no test can exist without being run.
- The CI workflow runs the bootloader C tests, the secure-link C tests and these documentation tests, installs the cryptography package, and triggers on push to main and on pull requests.
- .gitignore ignores __pycache__ and *.pyc, and no bytecode file is tracked by git.
<!-- api:doctests:end -->

## 5. Limits of this reference

- Code sizes quoted in the README (for example the bootloader's 10.3 KB) and timings measured on the chip are not checked here: CI builds nothing for the ARM target.
- Behaviour that needs the board (option-byte write protection, read-out protection, real flash timing) is described in the README, with evidence in `docs/secure_boot/`. These
  tests do not repeat it.
- The examples are compiled on the PC; they were not built into firmware.
