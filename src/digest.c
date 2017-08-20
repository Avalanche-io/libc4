
#include <openssl/evp.h>

// Digest represents a 64 byte "C4 Digest", which is the SHA-512 hash. Amongst
// other things Digest enforces padding to insure alignment with the original
// 64 byte hash.
//
// A digest is simply an array of 64 bytes and can be use wherever the raw SHA
// hash might be needed.
typedef byte* c4_digest_t;

// NewDigest creates a Digest and initializes it with the argument, enforcing
// byte alignment by padding with 0 (zero) if needed. NewDigest will return nil
// if the argument is larger then 64 bytes. For performance NewDigest may
// not copy the data to a new slice, so copies must be made explicitly when
// needed.
c4_digest_t c4id_new_digest(unsigned char *b, int length) {
	c4_digest_t d = malloc(64);
	int offset = length-64;

	for (int i = 0;i<64;i++) {
		d[i] = 0;
		if (offset >= 0) {
			d[i] = b[offset];
		}
		offset++;
	}
	return d;
}

int compare(c4_digest_t l, c4_digest_t r) {
	for (int i = 0; i<64; i++) {
		if (l[i] > r[i]) {
			return 1;
		} else if (l[i] < r[i]) {
			return -1;
		}
	}
	return 0;
}

// Sum returns the digest of the receiver and argument combined. Insuring
// proper order. C4 Digests of C4 Digests are always identified by concatenating
// the bytes of the larger digest after the bytes of the lesser digest to form a
// block of 128 bytes which are then IDed.
c4_digest_t c4id_digest_sum(c4_digest_t l, c4_digest_t r) {
	c4_digest_t tmp;
	switch( compare(l, r) ) {
		case -1 :
			// If the left side is larger then they are already in order, do nothing
			break;
		case 1 : // If the right side is larger swap them
			tmp = l;
			l = r;
			r = tmp;
			break;
		case 0 : // If they are identical no sum is needed, so just return one.
			return l;
			break;
	}

	EVP_MD_CTX *c = EVP_MD_CTX_create();
	EVP_DigestInit_ex(c, EVP_sha512(), NULL);
	EVP_DigestUpdate(c, l, 64);
	EVP_DigestUpdate(c, r, 64);

	unsigned char *md = malloc(64);
	EVP_DigestFinal_ex(c, md, NULL);
	return c4id_new_digest(md, 64);
}

// ID returns the C4 ID representation of the digest by directly translating the byte
// slice to the standard C4 ID format (the bytes are not (re)hashed).
c4_id_t* c4id_digest_id(c4_digest_t d) {
	big_int_t *id = new_big_int(512);
	big_int_set_bytes(id, d, 64);
	return (c4_id_t*) id;
}
