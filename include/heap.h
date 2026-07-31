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

uint8_t *heap_start;
uint8_t *heap_end;
uint8_t *heap_current;

extern block_t *block_head;
extern block_t *block_current;
extern block_t *block_tail;

extern void heap_init(multiboot_info_t *mbi);
extern void *heap_alloc(size_t size);
extern void *heap_realloc(void *ptr, size_t size);
extern void *heap_calloc(size_t base, size_t size);
extern void heap_free(void *ptr);
extern size_t heap_free_bytes();

#endif
