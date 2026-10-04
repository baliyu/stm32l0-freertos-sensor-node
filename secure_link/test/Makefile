# Host (PC) tests for secure_link. Run:  make && ./test_host && ./test_fcnt
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -O2 -I..

all: test_host test_fcnt

test_host: ../aes128.c ../aes_cmac.c ../secure_link.c test_host.c ../aes128.h ../aes_cmac.h ../secure_link.h
	$(CC) $(CFLAGS) -o $@ ../aes128.c ../aes_cmac.c ../secure_link.c test_host.c

test_fcnt: ../fcnt_store.c test_fcnt.c ../fcnt_store.h
	$(CC) $(CFLAGS) -o $@ ../fcnt_store.c test_fcnt.c

clean:
	rm -f test_host test_fcnt
