#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_DIR_SLOT(x) ((x) >> 22)
#define PAGE_TABLE_SLOT(x) (((x) >> 12) & 0x3FF)
#define PAGE_INC(addr, i) (addr + i * 0x400000)
#define PAGE_COUNT(size) ((size + 0x3FFFFF) / 0x400000)

extern uint32_t page_directory[1024] __attribute__((aligned(4096)));

extern void pages_init();
extern void page_map_physical(uint32_t addr, uint32_t *table);
extern void page_map_virtual(uint32_t virt, uint32_t phys, uint32_t *table);
extern void page_unmap(uint32_t virt);
extern void page_flush();

#endif
