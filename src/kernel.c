#define SERIAL_ENABLE_ALIAS

#include <stdint.h>
#include "multiboot.h"
#include "serial.h"
#include "string.h"
#include "heap.h"

uintptr_t __stack_chk_guard;

__attribute__((noreturn))
void abort() {
    __asm__ volatile("cli; hlt");
    __builtin_unreachable();
}

__attribute__((noreturn))
void panic(const char *msg) {
    serial_writeln(msg);
    abort();
    __builtin_unreachable();
}

__attribute__((noreturn))
void __stack_chk_fail() {
    panic("KERNEL PANIC: stack smashing detected");
    __builtin_unreachable();
}

__attribute__((noreturn))
void main(uint32_t magic, multiboot_info_t *mbi) {
    serial_init();
    serial_writeln("Mango kernel");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("KERNEL PANIC: invalid magic number");
    if (!(mbi->flags & (1 << 12)))
        panic("KERNEL PANIC: no video");

    heap_init(mbi->mem_upper);

    // hang
    while(1)
        abort();
}
