#include <stdint.h>
#include <stddef.h>

uint8_t *heap_start;
uint8_t *heap_end;
uint8_t *heap_current;

void heap_init(uint32_t mem_upper);
void *heap_alloc(size_t size);
void heap_free(void *ptr);
