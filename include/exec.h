#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

#define EXEC_BASE 0xA0000000
#define EXEC_MAGIC 0x58474E4D

typedef struct {
    uint32_t magic;
    uint32_t entry;
} exec_t;

typedef uint32_t (*exec_func_t)(void);
uint32_t exec_call(uint32_t entry);

#endif
