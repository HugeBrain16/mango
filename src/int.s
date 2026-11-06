.intel_syntax noprefix

.extern exception_handler
.extern irq_handler
.extern isr_count

.macro ISR_EXCEPTION num has_error
.global isr\num
isr\num:
.if !\has_error
    push 0
.endif
    push \num

    pushad

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call exception_handler
    add esp, 4
    
    pop gs
    pop fs
    pop es
    pop ds

    popad

    add esp, 8
    iret
.endm

.macro ISR num
.global isr\num
isr\num:
    push 0
    push \num

    pushad

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call isr_handler
    add esp, 4
    
    pop gs
    pop fs
    pop es
    pop ds

    popad

    add esp, 8
    iret
.endm

.macro IRQ num irq_num
.global irq\num
irq\num:
    push 0
    push \irq_num

    pushad

    push ds
    push es
    push fs
    push gs

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp
    call irq_handler
    add esp, 4
    
    pop gs
    pop fs
    pop es
    pop ds

    popad

    add esp, 8
    iret
.endm

.section .text
ISR_EXCEPTION 0,  0 // Divide Error
ISR_EXCEPTION 1,  0 // Debug
ISR_EXCEPTION 2,  0 // NMI
ISR_EXCEPTION 3,  0 // Breakpoint
ISR_EXCEPTION 4,  0 // Overflow
ISR_EXCEPTION 5,  0 // BOUND Range Exceeded
ISR_EXCEPTION 6,  0 // Invalid Opcode
ISR_EXCEPTION 7,  0 // Device Not Available
ISR_EXCEPTION 8,  1 // Double Fault
ISR_EXCEPTION 9,  0 // Coprocessor Segment Overrun (obsolete)
ISR_EXCEPTION 10, 1 // Invalid TSS
ISR_EXCEPTION 11, 1 // Segment Not Present
ISR_EXCEPTION 12, 1 // Stack-Segment Fault
ISR_EXCEPTION 13, 1 // General Protection Fault
ISR_EXCEPTION 14, 1 // Page Fault
ISR_EXCEPTION 15, 0 // Reserved
ISR_EXCEPTION 16, 0 // x87 Floating-Point Error
ISR_EXCEPTION 17, 1 // Alignment Check
ISR_EXCEPTION 18, 0 // Machine Check
ISR_EXCEPTION 19, 0 // SIMD Floating-Point Exception
ISR_EXCEPTION 20, 0 // Virtualization Exception
ISR_EXCEPTION 21, 1 // Control Protection Exception
ISR_EXCEPTION 22, 0 // Reserved
ISR_EXCEPTION 23, 0 // Reserved
ISR_EXCEPTION 24, 0 // Reserved
ISR_EXCEPTION 25, 0 // Reserved
ISR_EXCEPTION 26, 0 // Reserved
ISR_EXCEPTION 27, 0 // Reserved
ISR_EXCEPTION 28, 0 // Hypervisor Injection Exception
ISR_EXCEPTION 29, 0 // VMM Communication Exception
ISR_EXCEPTION 30, 1 // Security Exception
ISR_EXCEPTION 31, 0 // Reserved

IRQ 0, 32   // Timer
IRQ 1, 33   // Keyboard
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44  // PS/2 Mouse
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

ISR 48 // calls

.section .rodata
.global exception_stub_table
exception_stub_table:
.long isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
.long isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
.long isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
.long isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31

.global irq_stub_table
irq_stub_table:
.long irq0, irq1, irq2, irq3, irq4, irq5, irq6, irq7
.long irq8, irq9, irq10, irq11, irq12, irq13, irq14, irq15

.global isr_table
isr_table:
.long isr48
.global isr_table_end
isr_table_end:
