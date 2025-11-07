
CC = clang
# Use pkg-config if available, otherwise use system defaults
CINCUDES = $(shell pkg-config --cflags openssl 2>/dev/null || echo "")
CFLAGS = -c

all: lib/libc4.a

lib/libc4.a: lib.c lib
	$(CC) -o $@ $< $(CINCUDES) $(CFLAGS)

lib:
	mkdir lib

clean:
	rm -f lib/libc4.a