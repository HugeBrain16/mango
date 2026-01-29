#ifndef IDT_H
#define IDT_H

#include <stdint.h>

typedef struct idt_entry {
    uint16_t isr_low;
    uint16_t selector;
    uint8_t zero;
    uint8_t type;
    uint16_t isr_high;
} __attribute__((packed)) idt_entry_t;

typedef struct idt_ptr {
    uint16_t size;
    uint32_t offset;
} __attribute__((packed)) idt_ptr_t;

typedef struct int_frame {
    uint32_t gs, fs, es, ds;
    uint32_t ebp, edi, esi, edx, ecx, ebx, eax;
    uint32_t vector, error_code;
    uint32_t eip, cs, eflags, esp, ss;
} __attribute__((packed)) int_frame_t;

void cli();
void sti();
void idt_init();
void set_idt_entry(uint8_t vector, void *isr, uint8_t flags);

#endif
