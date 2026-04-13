#ifndef RAND_H
#define RAND_H

#include <stdint.h>

extern uint32_t rand();
extern void srand(uint32_t seed);
extern int randrange(int min, int max);

#endif