#!/usr/bin/env python3
"""mkimage.py - wrap an application binary in the secure-boot image header.

Input : the app as a raw binary, linked to run at 0x08006200
        (arm-none-eabi-objcopy -O binary app.elf app.bin)
Output: <out>.bin  header + app
        <out>.hex  the same with addresses included (safer to flash: the
                   programmer cannot put it at the wrong address).
                   --slot A (default) places it at 0x08006000 to run directly;
                   --slot B places it at 0x08018000 as an UPDATE, which the
                   bootloader verifies and copies into slot A at the next reset.
                   The image bytes are identical either way: the app is always
                   linked to run from slot A.

Signing: with --key, the SHA-256 is signed with ECDSA P-256 and the 64-byte
signature (r || s, big-endian) goes into the header at offset 64. Without
--key the image is UNSIGNED and a stage-3 bootloader will refuse it.

Header format: see bootloader/image.h. This tool and the bootloader must agree
on every byte; the host test in bootloader/test checks that they do.
"""
import argparse
import hashlib
import struct
import sys

try:
    from cryptography.exceptions import InvalidSignature
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec, utils
except ImportError:
    sys.exit("error: needs the 'cryptography' package:  pip install cryptography")

IMG_MAGIC = 0x31484253          # 'S','B','H','1' in memory (little-endian)
IMG_HDR_VERSION = 1
IMG_HDR_SIZE = 0x200
IMG_FIXED_LEN = 20
SIG_OFFSET = 64
SLOT_A_BASE = 0x08006000
SLOT_B_BASE = 0x08018000
SLOT_SIZE = 0x12000             # 72 KB
APP_BASE = SLOT_A_BASE + IMG_HDR_SIZE
IMG_MAX_SIZE = SLOT_SIZE - IMG_HDR_SIZE
RAM_START, RAM_END = 0x20000000, 0x20000000 + 20 * 1024


def parse_version(text):
    parts = text.split(".")
    if len(parts) != 3:
        raise ValueError("version must look like 1.2.3")
    major, minor, patch = (int(p) for p in parts)
    if not (0 <= major <= 255 and 0 <= minor <= 255 and 0 <= patch <= 65535):
        raise ValueError("version out of range (255.255.65535 max)")
    return (major << 24) | (minor << 16) | patch


def check_vectors(app):
    sp, reset = struct.unpack_from("<II", app, 0)
    if not (RAM_START <= sp <= RAM_END and sp % 4 == 0):
        sys.exit(f"error: initial SP 0x{sp:08X} is not in RAM - is this the right binary?")
    if not (reset & 1 and APP_BASE <= reset < APP_BASE + len(app)):
        sys.exit(f"error: reset handler 0x{reset:08X} is not inside the app at 0x{APP_BASE:08X}.\n"
                 "       Was the app linked at 0x08006200 (check the linker script)?")
    return sp, reset


def build_header(app, version):
    fixed = struct.pack("<IIIII", IMG_MAGIC, IMG_HDR_VERSION, IMG_HDR_SIZE, len(app), version)
    assert len(fixed) == IMG_FIXED_LEN
    digest = hashlib.sha256(fixed + app).digest()
    header = bytearray(fixed + digest + bytes(12) + bytes(64) + bytes(IMG_HDR_SIZE - 128))
    assert len(header) == IMG_HDR_SIZE
    return header, digest


def sign_digest(key_path, digest):
    with open(key_path, "rb") as f:
        key = serialization.load_pem_private_key(f.read(), password=None)
    der = key.sign(digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    r, s = utils.decode_dss_signature(der)
    sig = r.to_bytes(32, "big") + s.to_bytes(32, "big")
    # Self-check: verify with the public half before writing anything out
    try:
        key.public_key().verify(der, digest, ec.ECDSA(utils.Prehashed(hashes.SHA256())))
    except InvalidSignature:
        sys.exit("error: signature self-check failed")
    n = key.public_key().public_numbers()
    pub = n.x.to_bytes(32, "big") + n.y.to_bytes(32, "big")
    return sig, hashlib.sha256(pub).hexdigest()[:8]


def write_ihex(path, data, base):
    """Intel HEX: type 04 sets the upper 16 address bits, type 00 carries data, 01 ends."""
    def record(rtype, addr, payload):
        body = bytes([len(payload), (addr >> 8) & 0xFF, addr & 0xFF, rtype]) + payload
        return ":" + body.hex().upper() + f"{(-sum(body)) & 0xFF:02X}\n"

    lines, upper = [], None
    for off in range(0, len(data), 16):
        addr = base + off
        if (addr >> 16) != upper:
            upper = addr >> 16
            lines.append(record(0x04, 0, struct.pack(">H", upper)))
        lines.append(record(0x00, addr & 0xFFFF, data[off:off + 16]))
    lines.append(record(0x01, 0, b""))
    with open(path, "w") as f:
        f.writelines(lines)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("app_bin", help="application binary linked at 0x08006200")
    ap.add_argument("--version", required=True, help="firmware version, e.g. 1.0.0")
    ap.add_argument("-o", "--out", default="slotA", help="output name without extension (default: slotA)")
    ap.add_argument("--key", help="private key (PEM) to sign with, e.g. keys/signing_key.pem")
    ap.add_argument("--slot", choices=["A", "B"], default="A",
                    help="A: run directly (0x08006000); B: install as an update (0x08018000)")
    ap.add_argument("--tamper", type=lambda s: int(s, 0), metavar="OFFSET",
                    help="TEST ONLY: flip one bit at this offset in the app AFTER hashing")
    ap.add_argument("--tamper-rehash", type=lambda s: int(s, 0), metavar="OFFSET",
                    help="TEST ONLY: flip one bit in the app and RECOMPUTE the hash, keeping the "
                         "old signature (what an attacker without the private key could do)")
    args = ap.parse_args()

    with open(args.app_bin, "rb") as f:
        app = f.read()
    if len(app) < 8 or len(app) > IMG_MAX_SIZE:
        sys.exit(f"error: app is {len(app)} bytes; must be 8..{IMG_MAX_SIZE}")
    sp, reset = check_vectors(app)

    try:
        version = parse_version(args.version)
    except ValueError as e:
        sys.exit(f"error: {e}")

    header, digest = build_header(app, version)
    key_id = None
    if args.key:
        sig, key_id = sign_digest(args.key, digest)
        header[SIG_OFFSET:SIG_OFFSET + 64] = sig
    image = bytearray(header + app)

    if args.tamper is not None:
        if not 0 <= args.tamper < len(app):
            sys.exit("error: --tamper offset is outside the app")
        image[IMG_HDR_SIZE + args.tamper] ^= 0x01
        print(f"WARNING: test image - bit 0 of app byte 0x{args.tamper:X} flipped after hashing")

    if args.tamper_rehash is not None:
        if not 0 <= args.tamper_rehash < len(app):
            sys.exit("error: --tamper-rehash offset is outside the app")
        image[IMG_HDR_SIZE + args.tamper_rehash] ^= 0x01
        new_digest = hashlib.sha256(bytes(image[:IMG_FIXED_LEN]) + bytes(image[IMG_HDR_SIZE:])).digest()
        image[IMG_FIXED_LEN:IMG_FIXED_LEN + 32] = new_digest
        print(f"WARNING: test image - app byte 0x{args.tamper_rehash:X} changed and the hash recomputed;"
              " the signature is the original one")

    with open(args.out + ".bin", "wb") as f:
        f.write(image)
    base = SLOT_A_BASE if args.slot == "A" else SLOT_B_BASE
    write_ihex(args.out + ".hex", bytes(image), base)

    print(f"app      : {args.app_bin} ({len(app)} bytes, SP=0x{sp:08X}, reset=0x{reset:08X})")
    print(f"version  : {args.version}")
    print(f"sha256   : {digest.hex()}")
    print(f"signature: " + (f"ECDSA P-256, key id {key_id}" if key_id else "NONE (unsigned image)"))
    print(f"written  : {args.out}.bin / {args.out}.hex  ({len(image)} bytes at 0x{base:08X}, slot {args.slot}"
          + (", update" if args.slot == "B" else "") + ")")


if __name__ == "__main__":
    main()
