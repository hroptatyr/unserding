/*** btea.c - simple block cipher, David Wheeler, Roger Needham */

#include <stdint.h>
#define DELTA 0x9e3779b9
#define MX 

struct btea_state_s {
	uint32_t y, z, sum, e;
};

static inline uint32_t
scramble(struct btea_state_s *st, uint32_t const key[], uint32_t p)
{
	return ((st->z >> 5 ^ st->y << 2) + (st->y >> 3 ^ st->z << 4)) ^
		((st->sum ^ st->y) + (key[(p & 3) ^ st->e] ^ st->z));
}

static void
btea_enc(uint32_t *data, size_t dlen, uint32_t const key[4])
{
	struct btea_state_s st[1];
	unsigned int rounds;

	rounds = 6 + 52 / dlen;
	st->sum = 0;
	st->z = data[dlen - 1];
	do {
		st->sum += DELTA;
		st->e = (st->sum >> 2) & 3;
		for (size_t p = 0; p < dlen - 1; p++) {
			st->y = data[p + 1],
				st->z = data[p] += scramble(st, key, p);
		}
		st->y = data[0];
		st->z = data[dlen - 1] += scramble(st, key, dlen - 1);
	} while (--rounds);
	return;
}

static void
btea_dec(uint32_t *data, size_t dlen, uint32_t const key[4])
{
	struct btea_state_s st[1];
	unsigned int rounds;

	rounds = 6 + 52 / dlen;
	st->sum = rounds * DELTA;
	st->y = data[0];
	do {
		st->e = (st->sum >> 2) & 3;
		for (size_t p = dlen - 1; p > 0; p--) {
			st->z = data[p - 1],
				st->y = data[p] -= scramble(st, key, p);
		}
		st->z = data[dlen - 1];
		st->y = data[0] -= scramble(st, key, 0);
	} while ((st->sum -= DELTA) != 0);
	return;
}
