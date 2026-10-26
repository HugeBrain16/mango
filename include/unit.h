#ifndef UNIT_H
#define UNIT_H

#include <stdint.h>

#define KB(x) ((x) << 10)
#define MB(x) ((x) << 20)

#define AS_KB(x) ((x) >> 10)
#define AS_MB(x) ((x) >> 20)

extern void unit_get_size(uint32_t size, char *buffer);
extern uint32_t ms_to_ticks(int ms);

#endif
