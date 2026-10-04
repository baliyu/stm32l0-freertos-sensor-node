# Host (PC) tests for secure_link. Run:  make && ./test_host
CC      = gcc
CFLAGS  = -std=c99 -Wall -Wextra -Werror -O2 -I..
SRCS    = ../aes128.c ../aes_cmac.c ../secure_link.c test_host.c

test_host: $(SRCS) ../aes128.h ../aes_cmac.h ../secure_link.h
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f test_host
