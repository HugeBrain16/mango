#include "kernel.h"
#include "idt.h"
#include "pic.h"
#include "io.h"
#include "string.h"
#include "terminal.h"
#include "color.h"
#include "keyboard.h"
#include "pit.h"
#include "rtc.h"

static idt_entry_t idt[256];
static idt_ptr_t idt_ptr;

extern void *isr_stub_table[];
extern void *irq_stub_table[];

void sti() {
    __asm__ volatile("sti");
}

void cli() {
    __asm__ volatile("cli");
}

void set_idt_entry(uint8_t vector, void *isr, uint8_t flags) {
    idt_entry_t *descriptor = &idt[vector];

    descriptor->isr_low = (uint32_t) isr & 0xffff;
    descriptor->selector = 0x08;
    descriptor->type = flags;
    descriptor->isr_high = (uint32_t) isr >> 16;
    descriptor->zero = 0;
}

void pic_remap(int offset1, int offset2) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);

    outb(PIC1_DATA, offset1);
    outb(PIC2_DATA, offset2);

    outb(PIC1_DATA, 4);
    outb(PIC2_DATA, 2);

    outb(PIC1_DATA, ICW4_8086);
    outb(PIC2_DATA, ICW4_8086);

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_unmask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t value = inb(port) & ~(1 << (irq % 8));
    outb(port, value);
}

void pic_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    uint8_t value = inb(port) | (1 << (irq % 8));
    outb(port, value);
}

void pic_eoi(uint8_t irq) {
    if (irq >= 8)
        outb(PIC2_COMMAND, PIC_EOI);
    outb(PIC1_COMMAND, PIC_EOI);
}

void idt_init() {
    idt_ptr.size = 0xfff;
    idt_ptr.offset = (uint32_t) &idt[0];

    for (uint8_t vector = 0; vector < 32; vector++) {
        set_idt_entry(vector, isr_stub_table[vector], 0x8E);
    }

    pic_remap(32, 40);

    for (uint8_t vector = 0; vector < 16; vector++) {
        set_idt_entry(32 + vector, irq_stub_table[vector], 0x8E);
    }

    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    __asm__ volatile(
        "lidt %0\n"
        "sti\n"
    :: "m"(idt_ptr));
}

void exception_handler(int_frame_t *frame) {
    const char *exceptions[] = {
        "Divide Error", "Debug", "NMI", "Breakpoint",
        "Overflow", "BOUND Range Exceeded", "Invalid Opcode", "Device Not Available",
        "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
        "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
        "x87 FPE", "Alignment Check", "Machine Check", "SIMD FPE",
        "Virtualization Exception", "Control Protection Exception"
    };
    
    char buff[128];
    if (frame->vector < 22) {
        strfmt(buff, "[ INFO ] Exception: %s\n", exceptions[frame->vector]);
    } else {
        strfmt(buff, "[ INFO ] Exception: Unknown (%d)\n", frame->vector);
    }
    log(buff);
    
    strfmt(buff, "[ INFO ] Vector: %d\n", frame->vector);
    log(buff);
    strfmt(buff, "[ INFO ] Error Code: 0x%x\n", frame->error_code);
    log(buff);

    if (frame->vector == 14) {
        uint32_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));

        strfmt(buff, "[ INFO ] CR2: 0x%x\n", cr2);
        log(buff);
    }

    strfmt(buff, "[ INFO ] EIP: 0x%x\n", frame->eip);
    log(buff);
    strfmt(buff, "[ INFO ] CS: 0x%x, EFLAGS: 0x%x\n", frame->cs, frame->eflags);
    log(buff); 
    strfmt(buff, "[ INFO ] EAX: 0x%x, EBX: 0x%x\n", frame->eax, frame->ebx);
    log(buff);
    strfmt(buff, "[ INFO ] ECX: 0x%x, EDX: 0x%x\n", frame->ecx, frame->edx);
    log(buff);
    strfmt(buff, "[ INFO ] ESI: 0x%x, EDI: 0x%x\n", frame->esi, frame->edi);
    log(buff);
    strfmt(buff, "[ INFO ] EBP: 0x%x, ESP: 0x%x\n", frame->ebp, frame->esp);
    log(buff);

    panic("[ PANIC ] Exception!");
}

void irq_handler(int_frame_t *frame) {
    uint8_t irq = frame->vector - 32;
    pic_eoi(irq);

    if (irq == 0) {
        pit_handle();
    } else if (irq == 1) {
        keyboard_handle();
    } else if (irq == 8) {
        rtc_handle();
    }
}
