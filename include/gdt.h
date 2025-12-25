#include <stdint.h>

typedef struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t flags;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed)) gdt_entry_t;

typedef struct gdt_descriptor {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed)) gdt_descriptor_t;

void gdt_init();
