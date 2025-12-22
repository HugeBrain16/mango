#include <stdint.h>
#include <stddef.h>

void heap_init(uint32_t mem_upper);
void *heap_alloc(size_t size);
void heap_free(void *ptr);
