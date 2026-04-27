#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>
#include "multiboot.h"

uint8_t *heap_start;
uint8_t *heap_end;
uint8_t *heap_current;

extern void heap_init(multiboot_info_t *mbi);
extern void *heap_alloc(size_t size);
extern void *heap_realloc(void *ptr, size_t size);
extern void *heap_calloc(size_t base, size_t size);
extern void heap_free(void *ptr);
extern size_t heap_free_bytes();

#endif
