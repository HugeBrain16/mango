#include "idt.h"
#include "string.h"
#include "terminal.h"
#include "color.h"

static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

extern void *isr_stub_table[];

void set_idt_entry(uint8_t vector, void *isr, uint8_t flags) {
    idt_entry_t *descriptor = &idt[vector];

    descriptor->isr_low = (uint32_t) isr & 0xffff;
    descriptor->selector = 0x08;
    descriptor->type = flags;
    descriptor->isr_high = (uint32_t) isr >> 16;
    descriptor->zero = 0;
}

void idt_init() {
    idt_ptr.size = 0xfff;
    idt_ptr.offset = (uint32_t) &idt[0];

    for (uint8_t vector = 0; vector < 32; vector++) {
        set_idt_entry(vector, isr_stub_table[vector], 0x8E);
    }

    __asm__ volatile(
        "lidt %0\n"
        "sti\n"
    :: "m"(idt_ptr));
}

__attribute__((noreturn))
void exception_handler(int_frame_t *frame) {
    const char *exceptions[] = {
        "Divide Error", "Debug", "NMI", "Breakpoint",
        "Overflow", "BOUND Range Exceeded", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
        "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
        "x87 FPE", "Alignment Check", "Machine Check", "SIMD FPE",
        "Virtualization Exception", "Control Protection Exception"
    };
    
    term_write("\n!!! EXCEPTION !!!\n", COLOR_RED, COLOR_BLACK);
    
    char buff[128];
    if (frame->vector < 22) {
        strfmt(buff, "Exception: %s\n", exceptions[frame->vector]);
    } else {
        strfmt(buff, "Exception: Unknown (%d)\n", frame->vector);
    }
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    
    strfmt(buff, "Vector: %d\n", frame->vector);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    
    strfmt(buff, "Error Code: 0x%x\n", frame->error_code);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    
    strfmt(buff, "EIP: 0x%x\n", frame->eip);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    
    strfmt(buff, "CS: 0x%x  EFLAGS: 0x%x\n", frame->cs, frame->eflags);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    
    strfmt(buff, "EAX: 0x%x  EBX: 0x%x\n", frame->eax, frame->ebx);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    
    strfmt(buff, "ECX: 0x%x  EDX: 0x%x\n", frame->ecx, frame->edx);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    
    strfmt(buff, "ESI: 0x%x  EDI: 0x%x\n", frame->esi, frame->edi);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    
    strfmt(buff, "EBP: 0x%x  ESP: 0x%x\n", frame->ebp, frame->esp);
    term_write(buff, COLOR_WHITE, COLOR_BLACK);
    
    __asm__ volatile("cli; hlt");
    __builtin_unreachable();
}
