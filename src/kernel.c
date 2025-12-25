#define SERIAL_ENABLE_ALIAS

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

static void handle_command(const char *command, void *data) {
    char cmd[128] = {0};
    char args[32][128] = {0};
    int arg_count = 0;
    int idx = 0;
    int in_cmd = 1;

    for (size_t i = 0; command[i] != '\0'; i++) {
        if (command[i] == ' ') {
            if (in_cmd) {
                cmd[idx] = '\0';
                in_cmd = 0;
                idx = 0;
            } else if (idx > 0) {
                args[arg_count][idx] = '\0';
                arg_count++;
                if (arg_count >= 32) break;
                idx = 0;
            }
        } else {
            if (in_cmd) {
                cmd[idx++] = command[i];
            } else {
                args[arg_count][idx++] = command[i];
            }
        }
    }
 
    if (!in_cmd && idx > 0) {
        args[arg_count][idx] = '\0';
        arg_count++;
    }

    if (!strcmp(cmd, "scaleup")) {
        term_scale++;
    } else if (!strcmp(cmd, "scaledown")) {
        if (term_scale > 1)
            term_scale--;
    } else if (!strcmp(cmd, "clear")) {
        screen_clear(COLOR_BLACK);
        term_x = 0;
        term_y = 0;
    } else if (!strcmp(cmd, "exit")) {
        term_write("Halting...\n", COLOR_WHITE, COLOR_BLACK);
        abort();
    } else if (!strcmp(cmd, "fetch")) {
        char buff[32];
        multiboot_info_t *mbi = (multiboot_info_t*) data;

        term_write("\n", COLOR_BLACK, COLOR_BLACK);
        for (int i = 0; i < 6; i++) {
            term_write("=", COLOR_YELLOW, COLOR_BLACK);
            term_write("=", COLOR_WHITE, COLOR_BLACK);
            term_write("=", COLOR_YELLOW, COLOR_BLACK);
        }
        term_write("\n", COLOR_BLACK, COLOR_BLACK);
        term_write("Kernel: Mango\n", COLOR_WHITE, COLOR_BLACK);
        if (mbi) {
            strfmt(buff, "Memory: %d MB\n", (mbi->mem_upper >> 10) + 1);
            term_write(buff, COLOR_WHITE, COLOR_BLACK);
        }

        term_write("\n", COLOR_BLACK, COLOR_BLACK);
        term_write("=", COLOR_YELLOW, COLOR_YELLOW);
        term_write("=", COLOR_WHITE, COLOR_WHITE);
        term_write("=", COLOR_PURPLE, COLOR_PURPLE);
        term_write("=", COLOR_DARKGRAY, COLOR_DARKGRAY);
        term_write("\n", COLOR_BLACK, COLOR_BLACK);
    } else if (!strcmp(cmd, "echo")) {
        for (int i = 0; i < arg_count; i++) {
            term_write(args[i], COLOR_WHITE, COLOR_BLACK);
            term_write(" ", COLOR_BLACK, COLOR_BLACK);
        }
        term_write("\n", COLOR_BLACK, COLOR_BLACK);
    } else {
        term_write("Unknown command\n", COLOR_WHITE, COLOR_BLACK);
    }
}

__attribute__((noreturn))
void main(uint32_t magic, multiboot_info_t *mbi) {
    serial_init();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("KERNEL PANIC: invalid magic number");
    if (!(mbi->flags & (1 << 12)))
        panic("KERNEL PANIC: no video");

    gdt_init();
    heap_init(mbi->mem_upper);
    screen_init(mbi);
    term_init();

    term_write("Welcome to Mango!\n", COLOR_YELLOW, COLOR_BLACK);
    term_write("\n> ", COLOR_WHITE, COLOR_BLACK);
    term_mode = TERM_MODE_TYPE;

    while(1) {
        uint8_t scancode = ps2_read();
        char c = ps2_get_char(scancode);
        char s[2] = {c, '\0'};

        if (c > 0 && scancode != 0x80 && term_mode == TERM_MODE_TYPE) {
            term_write(s, COLOR_WHITE, COLOR_BLACK);
            
            if (c != '\b' && c != '\n') {
                term_input[term_input_cursor++] = c;
                term_input[term_input_cursor] = '\0';
            }

            if (c == '\n') {
                handle_command(term_input, mbi);
                term_input_cursor = 0;
                term_input[0] = '\0';
                term_write("\n> ", COLOR_WHITE, COLOR_BLACK);
            }
        }    
    }
}
