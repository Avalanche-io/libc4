
CC = clang
CLIBS = -L../lib -lc4 -lgmp -lssl -lcrypto -L/usr/local/Cellar/openssl/1.0.2l/lib/
CINCUDES = -I../include -I/usr/local/Cellar/openssl/1.0.2l/include
CFLAGS = -O2
# -mmacosx-version-min=10.12

all: testing

testing: testing.c error.c id_test.c
	$(CC) -o $@ testing.c  $(CLIBS) $(CINCUDES) $(CFLAGS)

clean:
	rm -f testing