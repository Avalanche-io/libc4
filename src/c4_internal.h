#ifndef C4_INTERNAL_H
#define C4_INTERNAL_H

#include "../include/big.h"
#include <openssl/evp.h>

typedef unsigned char byte;

/* Internal structure definitions - shared across implementation files */

struct c4_id_s {
	big_int_t bigint;
};

struct c4_digest_s {
	byte data[64];
};

struct c4id_encoder_s {
	EVP_MD_CTX* ctx;
};

#endif /* C4_INTERNAL_H */
