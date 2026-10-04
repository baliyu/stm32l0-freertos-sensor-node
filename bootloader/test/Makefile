# Host (PC) tests for the bootloader's SHA-256 and image check.
# Run:  make test
CC      = gcc
CFLAGS  = -std=gnu11 -Wall -Wextra -Werror -O2 -I..
TOOL    = ../../tools/mkimage.py

test: test_sha256 test_image fixture_good.bin
	./test_sha256
	@echo "--- cross-check against Python hashlib (random lengths 0..300) ---"
	@for n in 0 1 55 56 63 64 65 119 120 127 128 200 300; do \
	  head -c $$n /dev/urandom > rnd.bin; \
	  c=$$(./test_sha256 hashme < rnd.bin); \
	  p=$$(python3 -c "import hashlib,sys;print(hashlib.sha256(open('rnd.bin','rb').read()).hexdigest())"); \
	  if [ "$$c" = "$$p" ]; then echo "[PASS] $$n bytes"; else echo "[FAIL] $$n bytes"; exit 1; fi; \
	done; rm -f rnd.bin
	./test_image

test_sha256: ../sha256.c ../sha256.h test_sha256.c
	$(CC) $(CFLAGS) -o $@ ../sha256.c test_sha256.c

test_image: ../image.c ../image.h ../sha256.c test_image.c
	$(CC) $(CFLAGS) -o $@ ../image.c ../sha256.c test_image.c

# A fake 40 KB app with a plausible vector table (SP top of RAM, reset in slot A)
fake_app.bin:
	python3 -c "import struct,random;random.seed(1);d=struct.pack('<II',0x20005000,0x08008009)+bytes(random.getrandbits(8) for _ in range(40*1024-8));open('fake_app.bin','wb').write(d)"

fixture_good.bin: fake_app.bin $(TOOL)
	python3 $(TOOL) fake_app.bin --version 1.2.3 -o fixture_good > /dev/null
	python3 $(TOOL) fake_app.bin --version 1.2.3 -o fixture_tampered --tamper 0x1234 > /dev/null

clean:
	rm -f test_sha256 test_image fake_app.bin fixture_* rnd.bin
