#include <stdio.h>
#include <string.h>

#include "c4.h"
#include "big.h"

#include "error.c"

// the c4 id of "hello, world!":
const char *expected = "c43AQnB9bDGEwZSsxT1HwhHrCYXDoTrr4hmAJypFwxhTVngqUhVJwQtmJpcmb4Mq9Y6249ZzqNtWRZ8CE2gGSPRc6F";

int test_count = 3;
error* (*tests[4]) ();
// error* setup();
// error* teardown();

error* setup() {
	// First we must initialize the c4 library
	c4_init();
	return NULL;
}

error* teardown() {
	return NULL;
}

// C4 id of all FFFF:
const char *idFFFF = "c467rpwLCuS5DGA8KGZXKsVQ7dnPb9goRLoKfgGbLfQg9WoLUgNY77E2jT11fem3coV9nAkguBACzrU1iyZM4B8roQ";

error* TestAllFFFF() {
	printf("TestAllFFFF\n");
	unsigned char b[64];
	for (int i = 0; i < 64; i++) {
		b[i] = 255;
	}
	big_int_t *bignum = new_big_int(0);
	big_int_set_bytes(bignum, b, 64);

	c4_id_t *id	= (c4_id_t *)bignum;
	char *idstr = c4id_string(id);

	if (strcmp( idstr, idFFFF) != 0) {
		char *errstr = malloc(2048);
		snprintf(errstr, 2048, "AllFFFF ids do not match\n");
		snprintf(errstr, 2048, "%s\tgot:      \"%s\"\n", errstr, idstr);
		snprintf(errstr, 2048, "%s\texpected: \"%s\"\n", errstr, idFFFF);
		return NewError(errstr);
	}

	c4_id_t *id2 = c4id_parse("c467rpwLCuS5DGA8KGZXKsVQ7dnPb9goRLoKfgGbLfQg9WoLUgNY77E2jT11fem3coV9nAkguBACzrU1iyZM4B8roQ");
	if (id2 == NULL) {
		char *errstr = malloc(2048);
		snprintf(errstr, 2048, "AllFFFF failed to parse c4 id string.\n");
		return NewError(errstr);
	}

	big_int_t *bignum2 = new_big_int(0);
	big_int_set(bignum2, id2);
	// big_int_set(bignum2, id2);
	// unsigned char b2[64];
	// for ( int i = 0; i<64; i++ ) {
	// 	b2[i] = 0;
	// }
	unsigned char b2[64];
	// b2 = NULL;

	int n = big_int_get_bytes(bignum2, b2, 64);
	if (n == 0) {
		char *errstr = malloc(2048);
		snprintf(errstr, 2048, "TestAllFFFF c4id not parsed correctly\n");
		return NewError(errstr);
	}
	for ( int i = 0; i<64; i++ ) {
		if ( b2[i] != 0xFF ) {
			char *errstr = malloc(2048);
			snprintf(errstr, 2048, "TestAllFFFF c4id not parsed correctly\n");
			return NewError(errstr);
		}
	}
	return NULL;
}

const char *id0000 = "c41111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";

error* TestAll0000() {
	printf("TestAll0000\n");
	unsigned char b[64];
	for (int i = 0; i < 64; i++) {
		b[i] = 0;
	}
	big_int_t *bignum = new_big_int(0);
	big_int_set_bytes(bignum, b, 64);

	c4_id_t *id	= (c4_id_t *)bignum;
	char *idstr = c4id_string(id);

	if ( strcmp(idstr, id0000) != 0 ) {
		char *errstr = malloc(2048);
		snprintf(errstr, 2048, "All0000 ids do not match\n");
		snprintf(errstr, 2048, "%s\tgot:      \"%s\"\n", errstr, idstr);
		snprintf(errstr, 2048, "%s\texpected: \"%s\"\n", errstr, id0000);
		return NewError(errstr);
	}

	c4_id_t *id2 = c4id_parse("c41111111111111111111111111111111111111111111111111111111111111111111111111111111111111111");
	if (id2 == NULL) {
		char *errstr = malloc(2048);
		snprintf(errstr, 2048, "All0000 failed to parse c4 id string.\n");
		return NewError(errstr);
	}

	big_int_t *bignum2;
	big_int_set(bignum2, id2);
	unsigned char b2[64];
	for ( int i = 0; i<64; i++ ) {
		b2[i] = 0;
	}
	int n = big_int_get_bytes(bignum2, b2, 64);
	if (n != 0) {
		char *errstr = malloc(2048);
		snprintf(errstr, 2048, "TestAll0000 c4id not parsed correctly\n");
		return NewError(errstr);
	}

	for ( int i = 0; i<64; i++ ) {
		if (b2[i] != 0) {
			char *errstr = malloc(2048);
			snprintf(errstr, 2048, "TestAll0000 c4id not parsed correctly\n");
			return NewError(errstr);
		}
	}

	return NULL;
}

error* TestAppendOrder() {
	printf("TestAppendOrder\n");

	unsigned char byteData[4][64] = {
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 58},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0d, 0x24},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0xfa, 0x28},
		{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xac, 0xad, 0x10},
	};

	char* expectedIDs[4] = {
		"c41111111111111111111111111111111111111111111111111111111111111111111111111111111111111121",
		"c41111111111111111111111111111111111111111111111111111111111111111111111111111111111111211",
		"c41111111111111111111111111111111111111111111111111111111111111111111111111111111111112111",
		"c41111111111111111111111111111111111111111111111111111111111111111111111111111111111121111",
	};

	for (int k = 0; k < 4; k++) {
		unsigned char * b = byteData[k];
		big_int_t *bignum = new_big_int(0);
		big_int_set_bytes(bignum, b, 64);

		c4_id_t *id	= (c4_id_t *)bignum;
		char *idstr = c4id_string(id);

		if ( strcmp(idstr, expectedIDs[k]) != 0 ) {
			char *errstr = malloc(2048);
			snprintf(errstr, 2048, "TestAppendOrder ids do not match\n");
			snprintf(errstr, 2048, "%s\tgot:      \"%s\"\n", errstr, idstr);
			snprintf(errstr, 2048, "%s\texpected: \"%s\"\n", errstr, expectedIDs[k]);
			return NewError(errstr);
		}

		c4_id_t *id2 = c4id_parse(expectedIDs[k]);
	  if (id2 == NULL) {
	  	char *errstr = malloc(2048);
	  	snprintf(errstr, 2048, "TestAppendOrder failed to parse c4 id string.\n");
	  	return NewError(errstr);
	  }

// 		bignum2 := big.Int(*id2)
// 		b = (&bignum2).Bytes()
// 		size := len(b)
// 		for size < 64 {
// 			b = append([]byte{0}, b...)
// 			size++
// 		}
// 		for i, bb := range b {
// 			is.Equal(bb, byteData[k][i])
// 		}
	}
	return NULL;
}

// func TestParseBytesID(t *testing.T) {
// 	is := is.New(t)

// 	for _, test := range []struct {
// 		In  string
// 		Err string
// 		Exp string
// 	}{
// 		{
// 			In:  `c43ucjRutKqZSCrW43QGU1uwRZTGoVD7A7kPHKQ1z4X1Ge8mhW4Q1gk48Ld8VFpprQBfUC8JNvHYVgq453hCFrgf9D`,
// 			Err: ``,
// 			Exp: "This is a pretend asset file, for testing asset id generation.\n",
// 		},
// 		{
// 			In:  `c430cjRutKqZSCrW43QGU1uwRZTGoVD7A7kPHKQ1z4X1Ge8mhW4Q1gk48Ld8VFpprQBfUC8JNvHYVgq453hCFrgf9D`,
// 			Err: `non c4 id character at position 3`,
// 			Exp: "",
// 		},
// 		{
// 			In:  ``,
// 			Err: `c4 ids must be 90 characters long, input length 0`,
// 			Exp: "",
// 		},
// 		{
// 			In:  `c430cjRutKqZSCrW43QGU1uwRZTGoVD7A7kPHKQ1z4X1Ge8mhW4Q1gk48Ld8VFpprQBfUC8JNvHYVgq453hCFrgf9`,
// 			Err: `c4 ids must be 90 characters long, input length 89`,
// 			Exp: "",
// 		},
// 	} {
// 		id, err := c4.Parse(test.In)
// 		if len(test.Err) != 0 {
// 			is.Err(err)
// 			is.Equal(err->Error(), test.Err)
// 		} else {
// 			expectedID := c4.Identify(strings.NewReader(test.Exp))
// 			is.NoErr(err)
// 			is.Equal(expectedID.Cmp(id), 0)
// 		}
// 	}
// }

// func TestIDLess(t *testing.T) {
// 	is := is.New(t)
// 	id1 := encode(strings.NewReader(`1`)) // c42yrSHMvUcscrQBssLhrRE28YpGUv9Gf95uH8KnwTiBv4odDbVqNnCYFs3xpsLrgVZfHebSaQQsvxgDGmw5CX1fVy
// 	id2 := encode(strings.NewReader(`2`)) // c42i2hTBA9Ej4nqEo9iUy3pJRRE53KAH9RwwMSWjmfaQN7LxCymVz1zL9hEjqeFYzxtxXz2wRK7CBtt71AFkRfHodu

// 	is.Equal(id1.Less(id2), false)
// }

// func TestIDCmp(t *testing.T) {
// 	is := is.New(t)
// 	id1 := encode(strings.NewReader(`1`)) // c42yrSHMvUcscrQBssLhrRE28YpGUv9Gf95uH8KnwTiBv4odDbVqNnCYFs3xpsLrgVZfHebSaQQsvxgDGmw5CX1fVy
// 	id2 := encode(strings.NewReader(`2`)) // c42i2hTBA9Ej4nqEo9iUy3pJRRE53KAH9RwwMSWjmfaQN7LxCymVz1zL9hEjqeFYzxtxXz2wRK7CBtt71AFkRfHodu

// 	is.Equal(id1.Cmp(id2), 1)
// 	is.Equal(id2.Cmp(id1), -1)
// 	is.Equal(id1.Cmp(id1), 0)

// }

// func TestCompareIDs(t *testing.T) {
// 	is := is.New(t)

// 	for _, test := range []struct {
// 		Id_A *c4.ID
// 		Id_B *c4.ID
// 		Exp  int
// 	}{
// 		{

// 			Id_A: encode(strings.NewReader("Test string")),
// 			Id_B: encode(strings.NewReader("Test string")),
// 			Exp:  0,
// 		},
// 		{
// 			Id_A: encode(strings.NewReader("Test string A")),
// 			Id_B: encode(strings.NewReader("Test string B")),
// 			Exp:  -1,
// 		},
// 		{
// 			Id_A: encode(strings.NewReader("Test string B")),
// 			Id_B: encode(strings.NewReader("Test string A")),
// 			Exp:  1,
// 		},
// 		{
// 			Id_A: encode(strings.NewReader("Test string")),
// 			Id_B: nil,
// 			Exp:  -1,
// 		},
// 	} {
// 		is.Equal(test.Id_A.Cmp(test.Id_B), test.Exp)
// 	}

// }

// func TestBytesToID(t *testing.T) {
// 	is := is.New(t)

// 	d := c4.Digest([]byte{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 58})
// 	id := d.ID()
// 	is.Equal(id.String(), "c41111111111111111111111111111111111111111111111111111111111111111111111111111111111111121")
// }

// // func TestSum(t *testing.T) {
// // 	is := is.New(t)

// // 	id1 := c4.Identify(strings.NewReader("foo"))
// // 	id2 := c4.Identify(strings.NewReader("bar"))

// // 	is.True(id2.Less(id1))

// // 	bts := append(id2.Digest(), id1.Digest()...)
// // 	expectedSum := c4.Identify(bts)

// // 	testSum := id1.Digest().Sum(id2.Digest())
// // 	is.Equal(expectedSum, testSum)
// // }

// func TestNILID(t *testing.T) {
// 	is := is.New(t)

// 	// ID of nothing constant
// 	nilid := c4.NIL_ID
// 	is.Equal(nilid.String(), "c459dsjfscH38cYeXXYogktxf4Cd9ibshE3BHUo6a58hBXmRQdZrAkZzsWcbWtDg5oQstpDuni4Hirj75GEmTc1sFT")
// }
