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

static void handle_command(const char *command) {
    size_t len = strlen(command);
    char cmd[128];
    char arg[128];
    int findargs = 0;
    list_t args;
    list_init(&args);

    for (size_t i = 0; i < len; i++) {
        if (command[i] == ' ') {
            if (findargs == 0) {
                cmd[i] = '\0';
                findargs = 1;
            } else if (findargs == 1) {
                if (strlen(arg) > 0) {
                    list_push(&args, arg);
                    arg[0] = '\0';
                }
            }
            continue;
        }

        if (findargs == 0) {
            cmd[i] = command[i];
        } else if (findargs == 1) {
            arg[i] = command[i]; 
        }
    }

    if (cmd[strlen(cmd) - 1] != '\0')
        cmd[strlen(cmd)] = '\0';

    if (strlen(arg) > 0) {
        list_push(&args, arg);
        arg[0] = '\0';
    }

    if (!strcmp(cmd, "scaleup")) {
        term_scale++;
    } else if (!strcmp(cmd, "scaledown")) {
        if (term_scale > 1)
            term_scale--;
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
                handle_command(term_input);
                term_input_cursor = 0;
                term_input[0] = '\0';
                term_write("\n> ", COLOR_WHITE, COLOR_BLACK);
            }
        }    
    }
}
