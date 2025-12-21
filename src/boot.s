.intel_syntax noprefix

.set ALIGN, 1<<0
.set MEMINFO, 1<<1
.set VIDEO, 1<<2
.set FLAGS, ALIGN | MEMINFO | VIDEO
.set MAGIC, 0x1BADB002
.set CHECKSUM, -(MAGIC + FLAGS)

.section .multiboot
.align 4
.long MAGIC
.long FLAGS
.long CHECKSUM

.long 0 // header_addr
.long 0 // load_addr
.long 0 // load_end_addr
.long 0 // bss_end_addr
.long 0 // entry_addr

.long 0 // linear framebuffer
.long 800 // width
.long 600 // height
.long 32 // bit depth

.section .bss
.align 16
stack_bottom:
    .skip 16384
stack_top:

.section .text
.global _start
_start:
    lea esp, [stack_top]
    push ebx
    push eax
    call main
    cli
1:
    hlt
    jmp 1b
