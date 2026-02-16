#include <stdint.h>
#include "multiboot.h"
#include "serial.h"
#include "string.h"
#include "heap.h"
#include "screen.h"
#include "terminal.h"
#include "color.h"
#include "ps2.h"
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "pic.h"
#include "pit.h"
#include "file.h"
#include "keyboard.h"
#include "editor.h"
#include "ata.h"
#include "rtc.h"

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
    term_write("KERNEL PANIC: stack smashing detected\n", COLOR_WHITE, COLOR_BLACK);
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

    gdt_init();
    idt_init();
    rtc_init();
    ps2_init();
    heap_init(mbi->mem_upper);
    screen_init(mbi);
    term_init();
    file_init();

    ata_init();
    uint8_t ata_status = inb(ATA_PORT_STATUS);
    if (ata_status == 0x00 || ata_status == 0xFF) {
        term_write("No primary drive.", COLOR_WHITE, COLOR_BLACK);
        abort();
    }

    uint8_t ata_id[512];
    ata_identify(ata_id);

    if (ata_id[0] & (1 << 15)) {
        term_write("Incompatible storage device.", COLOR_WHITE, COLOR_BLACK);
        abort();
    }

    pit_set_frequency(100); // 100 hz
    pic_unmask(0); // pit
    pic_unmask(1); // keyboard

    term_write("Welcome to Mango!\n", COLOR_YELLOW, COLOR_BLACK);
    term_write("\n> ", COLOR_WHITE, COLOR_BLACK);
    keyboard_mode = KEYBOARD_MODE_TERM;
    term_prompt = term_x;

    while(1) {
        __asm__ volatile("hlt");
    }
}
