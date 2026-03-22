#include "paging.h"
#include "kernel.h"
#include "screen.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t first_page_table[1024] __attribute__((aligned(4096)));
static uint32_t screen_page_table[1024] __attribute__((aligned(4096)));

static void page_load_table(uint32_t addr, uint32_t *table) {
	uint32_t base = addr & 0xFFC00000; // align 4mb

	for (int i = 0; i < 1024; i++)
		table[i] = (base + i * 0x1000) | 3;
}

void page_map_physical(uint32_t addr, uint32_t *table) {
	page_load_table(addr, table);
	page_directory[PAGE_DIR_SLOT(addr)] = ((uint32_t)table) | 3;
}

void pages_init() {
	for (int i = 0; i < 1024; i++)
		page_directory[i] = 0x00000002;

	page_map_physical(0, first_page_table);
	page_map_physical((uint32_t)screen_buffer, screen_page_table);

	log("[ INFO ] Loading page directory...\n");
	__asm__ volatile(
		"mov %0, %%cr3\n"
	:: "r"(page_directory));

	log("[ INFO ] Enabling paging...\n");
	__asm__ volatile(
		"mov %%cr0, %%eax\n"
		"or $0x80000000, %%eax\n"
		"mov %%eax, %%cr0\n"
	:: : "eax");
}