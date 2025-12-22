#include "heap.h"

#define KB(x) ((x) << 10)
#define MB(x) ((x) << 20)

typedef struct block {
    size_t size;
    int is_free;
    struct block *next;
} block_t;

static uint8_t *heap_start;
static uint8_t *heap_end;
static uint8_t *heap_current;
static block_t *block_current;

static void block_append(block_t *block) {
    block_t *tail = block_current;
    while (tail->next)
        tail = tail->next;
    tail->next = block;
}

void heap_init(uint32_t mem_upper) {
    extern uint8_t _kernel_end;

    heap_start = &_kernel_end;
    uint32_t total_mem = KB(mem_upper) + MB(1);
    heap_end = (uint8_t *) total_mem - MB(1);
    heap_current = heap_start;
}

void *heap_alloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 15) & ~15; // align 16

    block_t *current = block_current;
    while (current) {
        if (current->is_free && current->size >= size) {
            current->is_free = 0;

            if (current->size > size + sizeof(block_t) + 16) {
                block_t *block = (block_t *) current + sizeof(block_t) + size;
                block->size = current->size - size - sizeof(block_t);
                block->is_free = 1;
                block->next = current->next;

                current->size = size;
                current->next = block;
            }

            return (void *) current + 1;
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

    if (!block_current)
        block_current = block;
    else
        block_append(block);

    return (void *) block + 1;
}

void heap_free(void *ptr) {
    if (!ptr) return;

    block_t *block = (block_t *) ptr - 1;
    block->is_free = 1;

    block_t *current;
    while (current) {
        block_t *adjacent = current->next;
        if (current->is_free && adjacent && adjacent->is_free) {
            current->size += sizeof(block_t) + adjacent->size;
            current->next = adjacent->next;
        }
        current->next = adjacent;
    }
}
