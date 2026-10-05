# Host (PC) tests for the bootloader's SHA-256, ECDSA and image checks.
# Run:  make test      (needs Python 3 with the 'cryptography' package)
CC      = gcc
UECC    = ../third_party/micro-ecc
UECC_DEFS = -DuECC_SUPPORTS_secp160r1=0 -DuECC_SUPPORTS_secp192r1=0 -DuECC_SUPPORTS_secp224r1=0 \
            -DuECC_SUPPORTS_secp256k1=0 -DuECC_SUPPORTS_secp256r1=1 -DuECC_SUPPORT_COMPRESSED_POINT=0
CFLAGS  = -std=gnu11 -Wall -Wextra -Werror -O2 -I. -I.. -I$(UECC) $(UECC_DEFS)
TOOLS   = ../../tools

test: test_sha256 test_ecdsa test_image test_rollback
	./test_sha256
	@echo "--- cross-check against Python hashlib (lengths around the padding boundaries) ---"
	@for n in 0 1 55 56 63 64 65 119 120 127 128 200 300; do \
	  head -c $$n /dev/urandom > rnd.bin; \
	  c=$$(./test_sha256 hashme < rnd.bin); \
	  p=$$(python3 -c "import hashlib;print(hashlib.sha256(open('rnd.bin','rb').read()).hexdigest())"); \
	  if [ "$$c" = "$$p" ]; then echo "[PASS] $$n bytes"; else echo "[FAIL] $$n bytes"; exit 1; fi; \
	done; rm -f rnd.bin
	./test_ecdsa
	./test_image
	./test_rollback

test_sha256: ../sha256.c ../sha256.h test_sha256.c
	$(CC) $(CFLAGS) -o $@ ../sha256.c test_sha256.c

uECC.o: $(UECC)/uECC.c
	$(CC) $(CFLAGS) -Wno-unused-parameter -c -o $@ $<

test_ecdsa: test_ecdsa.c ../sha256.c uECC.o
	$(CC) $(CFLAGS) -o $@ test_ecdsa.c ../sha256.c uECC.o

test_image: test_image.c ../image.c ../image.h ../sha256.c uECC.o fixture_pubkey.h fixture_good.bin
	$(CC) $(CFLAGS) -o $@ test_image.c ../image.c ../sha256.c uECC.o

test_rollback: test_rollback.c ../rollback.c ../rollback.h
	$(CC) $(CFLAGS) -o $@ test_rollback.c ../rollback.c

# Throw-away test keys (never the real signing key)
fixture_key.pem fixture_pubkey.h:
	python3 $(TOOLS)/gen_signing_key.py --key fixture_key.pem --header fixture_pubkey.h > /dev/null
other_key.pem:
	python3 $(TOOLS)/gen_signing_key.py --key other_key.pem --header other_pubkey.h > /dev/null

# A fake 40 KB app with a plausible vector table (SP top of RAM, reset in slot A)
fake_app.bin:
	python3 -c "import struct,random;random.seed(1);d=struct.pack('<II',0x20005000,0x08008009)+bytes(random.getrandbits(8) for _ in range(40*1024-8));open('fake_app.bin','wb').write(d)"

fixture_good.bin: fake_app.bin fixture_key.pem other_key.pem $(TOOLS)/mkimage.py
	python3 $(TOOLS)/mkimage.py fake_app.bin --version 1.2.3 --key fixture_key.pem -o fixture_good > /dev/null
	python3 $(TOOLS)/mkimage.py fake_app.bin --version 1.2.3 -o fixture_unsigned > /dev/null
	python3 $(TOOLS)/mkimage.py fake_app.bin --version 1.2.3 --key other_key.pem -o fixture_otherkey > /dev/null
	python3 $(TOOLS)/mkimage.py fake_app.bin --version 1.2.3 --key fixture_key.pem -o fixture_tampered --tamper 0x1234 > /dev/null
	python3 $(TOOLS)/mkimage.py fake_app.bin --version 1.2.3 --key fixture_key.pem -o fixture_rehashed --tamper-rehash 0x1234 > /dev/null

clean:
	rm -f test_sha256 test_ecdsa test_image test_rollback uECC.o fake_app.bin fixture_* other_* rnd.bin
