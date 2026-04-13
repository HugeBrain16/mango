#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_DIR_SLOT(x) ((x) >> 22)
#define PAGE_TABLE_SLOT(x) (((x) >> 12) & 0x3FF)

extern uint32_t page_directory[1024] __attribute__((aligned(4096)));

extern void pages_init();
extern void page_map_physical(uint32_t addr, uint32_t *table);

#endif