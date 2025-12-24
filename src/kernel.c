#define SERIAL_ENABLE_ALIAS

#include <stdint.h>
#include "multiboot.h"
#include "serial.h"
#include "string.h"
#include "heap.h"
#include "screen.h"
#include "terminal.h"
#include "color.h"

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

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("KERNEL PANIC: invalid magic number");
    if (!(mbi->flags & (1 << 12)))
        panic("KERNEL PANIC: no video");

    heap_init(mbi->mem_upper);
    screen_init(mbi);
    term_init();

    term_write("Welcome to Mango!\n", COLOR_YELLOW, COLOR_BLACK);
    term_write("\n> ", COLOR_WHITE, COLOR_BLACK);

    // hang
    while(1)
        abort();
}
