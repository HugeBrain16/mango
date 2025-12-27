#include "gdt.h"

static gdt_entry_t gdt[5];
static gdt_descriptor_t gdt_ptr;

void gdt_init() {
    gdt[0] = (gdt_entry_t){0}; // null descriptor
    
    // kernel code segment
    gdt[1].limit_low = 0xffff;
    gdt[1].base_low = 0x0000;
    gdt[1].base_mid = 0x00;
    gdt[1].flags = 0x9A; // 10011010
    gdt[1].granularity = 0xCF; // 11001111
    gdt[1].base_high = 0x00;

    // kernel data segment
    gdt[2].limit_low = 0xffff;
    gdt[2].base_low = 0x0000;
    gdt[2].base_mid = 0x00;
    gdt[2].flags = 0x92; // 10010010
    gdt[2].granularity = 0xCF; // 11001111
    gdt[2].base_high = 0x00;

    gdt_ptr.limit = sizeof(gdt) - 1;
    gdt_ptr.base = (uint32_t) &gdt;

    __asm__ volatile(
        "lgdt %0\n"
        "ljmp $0x08, $reload_segments\n"
        "reload_segments:\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
    :: "m"(gdt_ptr) : "eax");
}
