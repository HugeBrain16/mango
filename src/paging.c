#include "paging.h"
#include "kernel.h"
#include "screen.h"

uint32_t page_directory[1024] __attribute__((aligned(4096)));
static uint32_t first_page_table[1024] __attribute__((aligned(4096)));
static uint32_t screen_page_table[1024] __attribute__((aligned(4096)));

static void page_load_table(uint32_t virt, uint32_t phys, uint32_t *table) {
    uint32_t first = (virt >> 12) & 0x3FF;
    uint32_t base = (phys & ~0xFFF) - first * 0x1000;

    for (int i = 0; i < 1024; i++)
        table[i] = (base + i * 0x1000) | 3;
}

void page_flush() {
	__asm__ volatile("mov %0, %%cr3" :: "r"(page_directory) : "memory");
}

void page_unmap(uint32_t virt) {
	page_directory[PAGE_DIR_SLOT(virt)] = 2;
	page_flush();
}

void page_map_physical(uint32_t addr, uint32_t *table) {
	page_load_table(addr, addr, table);
	page_directory[PAGE_DIR_SLOT(addr)] = ((uint32_t)table) | 3;
	page_flush();
}

void page_map_virtual(uint32_t virt, uint32_t phys, uint32_t *table) {
	page_load_table(virt, phys, table);
    page_directory[PAGE_DIR_SLOT(virt)] = ((uint32_t)table) | 3;
	page_flush();
}

void pages_init() {
	for (int i = 0; i < 1024; i++)
		page_directory[i] = 0x00000002;

	page_map_physical(0, first_page_table);
	page_map_physical((uint32_t)screen_buffer, screen_page_table);

	char *msg = "[ INFO ] Loading page directory...\n";
	if (boot_logging) {
		if (boot_log)
			string_puts(boot_log, msg);
		else
			strcat(early_boot, msg);
	}
	log(msg);
	page_flush();

	msg = "[ INFO ] Enabling paging...\n";
	if (boot_logging) {
		if (boot_log)
			string_puts(boot_log, msg);
		else
			strcat(early_boot, msg);
	}
	log(msg);
	__asm__ volatile(
		"mov %%cr0, %%eax\n"
		"or $0x80000000, %%eax\n"
		"mov %%eax, %%cr0\n"
	:: : "eax");
}
