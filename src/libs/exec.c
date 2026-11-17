#include "exec.h"

uint8_t exec_key = 0;

uint32_t exec_call(uint32_t entry) {
    uint32_t ret;

    __asm__ volatile(
        "push %%ebx\n"
        "push %%esi\n"
        "push %%edi\n"
        "push %%ebp\n"
        "call *%1\n"
        "pop %%ebp\n"
        "pop %%edi\n"
        "pop %%esi\n"
        "pop %%ebx\n"
        : "=a"(ret)
        : "r"((uint32_t)EXEC_BASE + entry)
        : "ecx", "edx", "memory"
    );

    return ret;
}

void exec_handle_type(uint8_t scancode) {
    exec_key = scancode;
}
