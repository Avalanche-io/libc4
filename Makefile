
CC = clang
CINCUDES = -I/usr/local/Cellar/openssl/1.0.2l/include
CFLAGS = -c
# -mmacosx-version-min=10.12

all: lib/libc4.a

lib/libc4.a: lib.c
	$(CC) -o $@ $< $(CINCUDES) $(CFLAGS)

clean:
	rm -f lib/libc4.a