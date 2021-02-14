
CC = clang
CINCUDES = -I/usr/local/Cellar/openssl@1.1/1.1.1i/include
CFLAGS = -c
# -mmacosx-version-min=10.12

all: lib/libc4.a

lib/libc4.a: lib.c lib
	$(CC) -o $@ $< $(CINCUDES) $(CFLAGS)

lib:
	mkdir lib

clean:
	rm -f lib/libc4.a