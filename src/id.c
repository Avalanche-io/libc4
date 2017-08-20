const char* charset = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";
const unsigned long base    = 58;

typedef big_int_t c4_id_t;
typedef unsigned char byte;

unsigned char lut[256];

void c4_init() {

	for (int i = 0; i < 256; i++) {
		lut[i] = (byte)255;
	}

	for (int i = 0; i < base; i++) {
		lut[charset[i]] = (byte)i;
	}

}

// c4id_release frees all allocations for the given c4 id.
void c4id_release(c4_id_t* id) {
	free(id);
}

// Parse parses a c4_id_t from a NULL terminated c-str.
c4_id_t* c4id_parse(const char* src) {
	c4_id_t *n = new_big_int(0);

	// bigBase := big.NewInt(base)
	for (int i = 2; i < 90; i++) {
		byte b = lut[src[i]];
		if (b == 0xFF) {
			return NULL;
		}
		big_int_mul(n, n, base);
		big_int_add(n, n, b);
	}
	return (c4_id_t*)n;
}

// String returns the standard string representation of a C4 id.
char* c4id_string(c4_id_t *id) {
	int mod;
	char *encoded = malloc(91);
	c4_id_t *n = new_big_int(512);
	c4_id_t *q = new_big_int(0);
	c4_id_t *zero = new_big_int(0);

	big_int_set(n, id);

	for (int i = 89;i>1;i--) {
		if (big_int_cmp(n, zero) == 0) {
			encoded[i] = '1';
			continue;
		}
		mod = big_int_div_mod(q, n, base);
		big_int_set(n, q);
		encoded[i] = charset[mod];
	}

	// c string terminator
	encoded[90] = 0;

	// prefix
	encoded[0] = 'c';
	encoded[1] = '4';
	return encoded;
}

// IDdigest returns the C4 Digest of `id`.
// Digest IDdigest(ID *id) {
// 	return NewDigest((*big.Int)(id).Bytes())
// }
