.intel_syntax noprefix

.extern exception_handler

.macro ISR num has_error
.global isr\num
isr\num:
.if !\has_error
    push 0
.endif
    push \num

    push eax
    push ebx
    push ecx
    push edx
    push esi
    push edi
    push ebp

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
    
    pop ebp
    pop edi
    pop esi
    pop edx
    pop ecx
    pop ebx
    pop eax
    
    add esp, 8
    iret
.endm

.section .text
ISR 0,  0 // Divide Error
ISR 1,  0 // Debug
ISR 2,  0 // NMI
ISR 3,  0 // Breakpoint
ISR 4,  0 // Overflow
ISR 5,  0 // BOUND Range Exceeded
ISR 6,  0 // Invalid Opcode
ISR 7,  0 // Device Not Available
ISR 8,  1 // Double Fault
ISR 9,  0 // Coprocessor Segment Overrun (obsolete)
ISR 10, 1 // Invalid TSS
ISR 11, 1 // Segment Not Present
ISR 12, 1 // Stack-Segment Fault
ISR 13, 1 // General Protection Fault
ISR 14, 1 // Page Fault
ISR 15, 0 // Reserved
ISR 16, 0 // x87 Floating-Point Error
ISR 17, 1 // Alignment Check
ISR 18, 0 // Machine Check
ISR 19, 0 // SIMD Floating-Point Exception
ISR 20, 0 // Virtualization Exception
ISR 21, 1 // Control Protection Exception
ISR 22, 0 // Reserved
ISR 23, 0 // Reserved
ISR 24, 0 // Reserved
ISR 25, 0 // Reserved
ISR 26, 0 // Reserved
ISR 27, 0 // Reserved
ISR 28, 0 // Hypervisor Injection Exception
ISR 29, 0 // VMM Communication Exception
ISR 30, 1 // Security Exception
ISR 31, 0 // Reserved

.section .rodata
.global isr_stub_table
isr_stub_table:
.long isr0, isr1, isr2, isr3, isr4, isr5, isr6, isr7
.long isr8, isr9, isr10, isr11, isr12, isr13, isr14, isr15
.long isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23
.long isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31
