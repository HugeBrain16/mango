#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>
#include "multiboot.h"

typedef struct block {
    size_t size;
    int is_free;
    struct block *next;
    struct block *prev;
} block_t;

typedef struct {
    uint8_t *heap_start;
    uint8_t *heap_end;
    uint8_t *heap_current;
    block_t *block_head;
    block_t *block_current;
    block_t *block_tail;
} heap_state_t;

extern uint8_t *heap_start;
extern uint8_t *heap_end;
extern uint8_t *heap_current;

extern block_t *block_head;
extern block_t *block_current;
extern block_t *block_tail;

extern void heap_init(multiboot_info_t *mbi);
extern void *heap_alloc(size_t size);
extern void *heap_realloc(void *ptr, size_t size);
extern void *heap_calloc(size_t base, size_t size);
extern void *heap_grow(void *ptr, size_t size);
extern void heap_free(void *ptr);
extern size_t heap_free_bytes();
extern size_t heap_used_bytes();
extern void heap_stat(size_t *used, size_t *usable, size_t *free, int *blocks);
extern void heap_get_state(heap_state_t *state);
extern void heap_set_state(heap_state_t *state);
#endif
