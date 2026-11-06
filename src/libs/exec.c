#include "exec.h"

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
