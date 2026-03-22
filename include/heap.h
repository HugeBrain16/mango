#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>
#include <stddef.h>
#include "multiboot.h"

uint8_t *heap_start;
uint8_t *heap_end;
uint8_t *heap_current;

void heap_init(multiboot_info_t *mbi);
void *heap_alloc(size_t size);
void *heap_realloc(void *ptr, size_t size);
void heap_free(void *ptr);

#endif
