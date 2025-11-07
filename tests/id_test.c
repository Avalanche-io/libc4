#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "c4.h"

#include "error.c"

// the c4 id of "hello, world!":
const char *expected = "c43AQnB9bDGEwZSsxT1HwhHrCYXDoTrr4hmAJypFwxhTVngqUhVJwQtmJpcmb4Mq9Y6249ZzqNtWRZ8CE2gGSPRc6F";

int test_count = 3;
error* (*tests[4]) ();

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

	// Create a digest from all 0xFF bytes
	unsigned char b[64];
	for (int i = 0; i < 64; i++) {
		b[i] = 255;
	}

	// Use digest API to convert bytes to ID
	c4_digest_t *digest = c4id_digest_new(b, 64);
	if (digest == NULL) {
		return NewError("Failed to create digest\n");
	}

	c4_id_t *id = c4id_digest_to_id(digest);
	if (id == NULL) {
		c4id_digest_free(digest);
		return NewError("Failed to convert digest to ID\n");
	}

	char *idstr = c4id_string(id);
	if (idstr == NULL) {
		c4id_free(id);
		c4id_digest_free(digest);
		return NewError("Failed to convert ID to string\n");
	}

	if (strcmp(idstr, idFFFF) != 0) {
		char *errstr = malloc(2048);
		snprintf(errstr, 2048, "AllFFFF ids do not match\n\tgot:      \"%s\"\n\texpected: \"%s\"\n",
			idstr, idFFFF);
		free(idstr);
		c4id_free(id);
		c4id_digest_free(digest);
		return NewError(errstr);
	}

	// Test parsing
	c4_id_t *id2 = c4id_parse(idFFFF);
	if (id2 == NULL) {
		free(idstr);
		c4id_free(id);
		c4id_digest_free(digest);
		return NewError("AllFFFF failed to parse c4 id string.\n");
	}

	// Compare the two IDs
	int cmp = c4id_cmp(id, id2);
	if (cmp != 0) {
		char *errstr = malloc(2048);
		snprintf(errstr, 2048, "TestAllFFFF: parsed ID doesn't match created ID (cmp=%d)\n", cmp);
		free(idstr);
		c4id_free(id);
		c4id_free(id2);
		c4id_digest_free(digest);
		return NewError(errstr);
	}

	free(idstr);
	c4id_free(id);
	c4id_free(id2);
	c4id_digest_free(digest);
	return NULL;
}

const char *id0000 = "c41111111111111111111111111111111111111111111111111111111111111111111111111111111111111111";

error* TestAll0000() {
	printf("TestAll0000\n");

	// Create a digest from all 0x00 bytes
	unsigned char b[64];
	for (int i = 0; i < 64; i++) {
		b[i] = 0;
	}

	// Use digest API to convert bytes to ID
	c4_digest_t *digest = c4id_digest_new(b, 64);
	if (digest == NULL) {
		return NewError("Failed to create digest\n");
	}

	c4_id_t *id = c4id_digest_to_id(digest);
	if (id == NULL) {
		c4id_digest_free(digest);
		return NewError("Failed to convert digest to ID\n");
	}

	char *idstr = c4id_string(id);
	if (idstr == NULL) {
		c4id_free(id);
		c4id_digest_free(digest);
		return NewError("Failed to convert ID to string\n");
	}

	if (strcmp(idstr, id0000) != 0) {
		char *errstr = malloc(2048);
		snprintf(errstr, 2048, "All0000 ids do not match\n\tgot:      \"%s\"\n\texpected: \"%s\"\n",
			idstr, id0000);
		free(idstr);
		c4id_free(id);
		c4id_digest_free(digest);
		return NewError(errstr);
	}

	// Test parsing
	c4_id_t *id2 = c4id_parse(id0000);
	if (id2 == NULL) {
		free(idstr);
		c4id_free(id);
		c4id_digest_free(digest);
		return NewError("All0000 failed to parse c4 id string.\n");
	}

	// Compare the two IDs
	int cmp = c4id_cmp(id, id2);
	if (cmp != 0) {
		char *errstr = malloc(2048);
		snprintf(errstr, 2048, "TestAll0000: parsed ID doesn't match created ID (cmp=%d)\n", cmp);
		free(idstr);
		c4id_free(id);
		c4id_free(id2);
		c4id_digest_free(digest);
		return NewError(errstr);
	}

	free(idstr);
	c4id_free(id);
	c4id_free(id2);
	c4id_digest_free(digest);
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
		unsigned char *b = byteData[k];

		// Use digest API to convert bytes to ID
		c4_digest_t *digest = c4id_digest_new(b, 64);
		if (digest == NULL) {
			return NewError("Failed to create digest\n");
		}

		c4_id_t *id = c4id_digest_to_id(digest);
		if (id == NULL) {
			c4id_digest_free(digest);
			return NewError("Failed to convert digest to ID\n");
		}

		char *idstr = c4id_string(id);
		if (idstr == NULL) {
			c4id_free(id);
			c4id_digest_free(digest);
			return NewError("Failed to convert ID to string\n");
		}

		if (strcmp(idstr, expectedIDs[k]) != 0) {
			char *errstr = malloc(2048);
			snprintf(errstr, 2048, "TestAppendOrder ids do not match (test %d)\n\tgot:      \"%s\"\n\texpected: \"%s\"\n",
				k, idstr, expectedIDs[k]);
			free(idstr);
			c4id_free(id);
			c4id_digest_free(digest);
			return NewError(errstr);
		}

		c4_id_t *id2 = c4id_parse(expectedIDs[k]);
		if (id2 == NULL) {
			free(idstr);
			c4id_free(id);
			c4id_digest_free(digest);
			return NewError("TestAppendOrder failed to parse c4 id string.\n");
		}

		// Compare IDs
		int cmp = c4id_cmp(id, id2);
		if (cmp != 0) {
			char *errstr = malloc(2048);
			snprintf(errstr, 2048, "TestAppendOrder: parsed ID doesn't match created ID (test %d, cmp=%d)\n", k, cmp);
			free(idstr);
			c4id_free(id);
			c4id_free(id2);
			c4id_digest_free(digest);
			return NewError(errstr);
		}

		free(idstr);
		c4id_free(id);
		c4id_free(id2);
		c4id_digest_free(digest);
	}

	return NULL;
}
