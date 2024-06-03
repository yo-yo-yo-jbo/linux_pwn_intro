CC=gcc
CFLAGS=-fno-stack-protector -w

chall: chall.c
	$(CC) $(CFLAGS) -o chall chall.c

clean:
	rm -f chall

