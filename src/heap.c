#include "heap.h"
#include "paging.h"
#include "string.h"

#define KB(x) ((x) << 10)
#define MB(x) ((x) << 20)

typedef struct block {
    size_t size;
    int is_free;
    struct block *next;
} block_t;

static uint32_t init_page_tables[4][1024] __attribute__((aligned(4096)));
static int table_index = 0;
static block_t *block_head;
static block_t *block_current;
static block_t *block_tail;

static uint32_t *alloc_init_table() {
    return init_page_tables[table_index++];
}

void heap_init(multiboot_info_t *mbi) {
    extern uint8_t _kernel_end;

    heap_start = &_kernel_end;
    heap_current = heap_start;

    multiboot_memory_map_t *entry = (multiboot_memory_map_t*)mbi->mmap_addr;
    multiboot_memory_map_t *end = (multiboot_memory_map_t*)(mbi->mmap_addr + mbi->mmap_length);

    while (entry < end) {
        if (entry->type == 1) {
            uint32_t base = (uint32_t)entry->addr;
            uint32_t top = base + (uint32_t)entry->len;

            heap_end = (uint8_t*)top;

            for (uint32_t addr = base & 0xFFC00000; addr < top; addr += 0x400000) {
                if (page_directory[PAGE_DIR_SLOT(addr)] & 1)
                    continue;

                uint32_t *table;
                if (table_index >= 4) {
                    void *ptr = heap_alloc(sizeof(uint32_t) * 1024 + 4096);
                    table = (uint32_t*)(((uint32_t)ptr + 4095) & ~4095);
                } else
                    table = alloc_init_table();
                page_map_physical(addr, table);
            }
        }

        entry = (multiboot_memory_map_t*)((uint32_t)entry + entry->size + 4);
    }
}

block_t *heap_header(void *ptr) {
    return (block_t *) ((uint8_t *) ptr - sizeof(block_t));
}

static void heap_split(block_t *block, size_t size) {
    size_t remainder = block->size - size;
    if (remainder < sizeof(block_t) + 16) return;

    block_t *sliced = (block_t *)((uint8_t *)block + sizeof(block_t) + size);
    sliced->size = remainder - sizeof(block_t);
    sliced->is_free = 1;
    sliced->next = block->next;

    block->size = size;
    block->next = sliced;

    if (block_tail == block)
        block_tail = sliced;
}

void *heap_alloc(size_t size) {
    if (size == 0) return NULL;
    size = (size + 15) & ~15; // align 16

    block_t *current = block_current ? block_current : block_head;
    block_t *start = current;
    int wrapped = 0;

    while (current) {
        if (current->is_free && current->size >= size) {
            current->is_free = 0;

            heap_split(current, size);
            block_current = current->next;
            return (uint8_t *) current + sizeof(block_t);
        }

        current = current->next;
        if (!current) {
            if (wrapped) break;

            current = block_head;
            wrapped = 1;
        }

        if (wrapped && current == start) break;
    }

    if (heap_current + (sizeof(block_t) + size) > heap_end) {
        return NULL; // Out of memory
    }

    block_t *block = (block_t *) heap_current;
    block->size = size;
    block->is_free = 0;
    block->next = NULL;
    heap_current += sizeof(block_t) + size;

    if (!block_head) {
        block_head = block;
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

    while (block->next && block->next->is_free) {
        if (block->next == block_tail)
            block_tail = block;
        if (block->next == block_current)
            block_current = block;
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
    size = (size + 15) & ~15;

    block_t *block = heap_header(ptr);
    if (block->size >= size) {
        heap_split(block, size);
        return ptr;
    }

    while (block->next && block->next->is_free) {
        if (block->next == block_tail) block_tail = block;
        if (block->next == block_current) block_current = block;

        block->size += sizeof(block_t) + block->next->size;
        block->next = block->next->next;

        if (block->size >= size) {
            heap_split(block, size);
            return ptr;
        }
    }

    void *new = heap_alloc(size);
    if (!new) return NULL;
    memcpy(new, ptr, block->size < size ? block->size : size);
    heap_free(ptr);
    return new;
}
