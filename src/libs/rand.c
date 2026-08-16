#include <rand.h>

static uint32_t state = 1;

uint32_t rand() {
	uint32_t x = state;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	state = x;
	return state;
}

void srand(uint32_t seed) {
	if (seed == 0)
		seed = 0xA5A5A5A5;
	state = seed;
}

int randrange(int min, int max) {
	return min + (rand() % (max - min + 1));
}