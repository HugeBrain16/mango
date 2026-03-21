#include "heap.h"
#include "string.h"

#define KB(x) ((x) << 10)
#define MB(x) ((x) << 20)

typedef struct block {
    size_t size;
    int is_free;
    struct block *next;
} block_t;

static block_t *block_current;
static block_t *block_tail;

void heap_init(uint32_t mem_upper) {
    extern uint8_t _kernel_end;

    heap_start = &_kernel_end;
    uint32_t total_mem = KB(mem_upper) + MB(1);
    heap_end = (uint8_t *) total_mem - MB(1);
    heap_current = heap_start;
}

block_t *heap_header(void *ptr) {
    return (block_t *) ((uint8_t *) ptr - sizeof(block_t));
}

void *heap_alloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 15) & ~15; // align 16

    block_t *current = block_current;
    while (current) {
        if (current->is_free && current->size >= size) {
            current->is_free = 0;

            if (current->size > size + sizeof(block_t) + 16) {
                block_t *block = (block_t *) ((uint8_t *) current + sizeof(block_t) + size);
                block->size = current->size - size - sizeof(block_t);
                block->is_free = 1;
                block->next = current->next;

                current->size = size;
                current->next = block;
            }

            return (uint8_t *) current + sizeof(block_t);
        }
        current = current->next;
    }

    if (heap_current + (sizeof(block_t) + size) > heap_end) {
        return NULL; // Out of memory
    }

    block_t *block = (block_t *) heap_current;
    block->size = size;
    block->is_free = 0;
    block->next = NULL;
    heap_current += sizeof(block_t) + size;

    if (!block_current) {
        block_current = block;
        block_tail = block;
    } else {
        block_tail->next = block;
        block_tail = block;
    }

    return (uint8_t *) block + sizeof(block_t);
}

void heap_free(void *ptr) {
    if (!ptr) return;

    block_t *block = heap_header(ptr);
    block->is_free = 1;

    if (block->next && block->next->is_free) {
        block->size += sizeof(block_t) + block->next->size;
        block->next = block->next->next;
    }
}

void *heap_realloc(void *ptr, size_t size) {
    if (!ptr) return heap_alloc(size);
    if (size == 0) {
        heap_free(ptr);
        return NULL;
    }

    block_t *block = heap_header(ptr);

    void *new = heap_alloc(size);
    if (!new) return NULL;

    size_t resize = (block->size < size) ? block->size : size;
    memcpy(new, ptr, resize);

    heap_free(ptr);
    return new;
}
