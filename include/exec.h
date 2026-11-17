#ifndef EXEC_H
#define EXEC_H

#include <stdint.h>

#define EXEC_BASE 0xA0000000
#define EXEC_MAGIC 0x58474E4D

typedef struct {
    uint32_t magic;
    uint32_t entry;
} exec_t;

extern uint8_t exec_key;

typedef uint32_t (*exec_func_t)(void);
extern uint32_t exec_call(uint32_t entry);
extern void exec_handle_type(uint8_t scancode);

#endif
