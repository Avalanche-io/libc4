// package id
#include <openssl/evp.h>
#include <gmp.h>

// Encoder generates an c4_id_t for a contiguous bock of data.
typedef struct {
	// SHA512_CTX *c;
	EVP_MD_CTX *c;
} c4id_encoder_t;

typedef big_int_t c4_id_t;
typedef unsigned char byte;

// NewEncoder makes a new Encoder.
c4id_encoder_t* c4id_new_encoder() {
	c4id_encoder_t* e = malloc(sizeof(c4id_encoder_t));
	e->c = EVP_MD_CTX_create();
	EVP_DigestInit_ex(e->c, EVP_sha512(), NULL);
	return e;
}

void c4id_release_encoder(c4id_encoder_t* e) {
	if (e != NULL) {
		if (e->c != NULL) {
			EVP_MD_CTX_destroy(e->c);
		}
		free(e);
	}
}

//  EncoderUpdate writes `size` bytes from `data` to the encoder `e`
void c4id_encoder_update(c4id_encoder_t *e, unsigned char* data, int size) {
	EVP_DigestUpdate(e->c, data, size);
}

// c4idEncodedID returns the c4_id_t for the bytes written to the encoder.
c4_id_t* c4id_encoded_id(c4id_encoder_t *e)  {
	byte* md = malloc(64);
	// SHA512_Final(md, e->c);
	EVP_DigestFinal_ex(e->c, md, NULL);
	big_int_t *n = new_big_int(512);

	big_int_set_bytes(n, md, 64);

	return (c4_id_t*)n;
}

// Digest get the Digest for the bytes written so far.
// func (e *Encoder) Digest() Digest {
// 	return NewDigest(e.h.Sum(nil));
// }

// Reset the encoder so it can identify a new block of data.
// func (e *Encoder) Reset() {
// 	e.h.Reset()
// }
