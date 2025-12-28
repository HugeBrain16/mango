#include <cpuid.h>
#include "command.h"
#include "kernel.h"
#include "string.h"
#include "heap.h"
#include "screen.h"
#include "terminal.h"
#include "color.h"
#include "time.h"

void command_handle(const char *command) {
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
        char buff[128];

        term_write("\n", COLOR_BLACK, COLOR_BLACK);
        for (int i = 0; i < 6; i++) {
            term_write("=", COLOR_YELLOW, COLOR_BLACK);
            term_write("=", COLOR_WHITE, COLOR_BLACK);
            term_write("=", COLOR_YELLOW, COLOR_BLACK);
        }
        term_write("\n", COLOR_BLACK, COLOR_BLACK);
        term_write("Kernel: Mango\n", COLOR_WHITE, COLOR_BLACK);

        uint32_t eax, ebx, ecx, edx;
        char cpu_name[64] = {0};
        uint32_t *p = (uint32_t *) cpu_name;

        __cpuid(0x80000000, eax, ebx, ecx, edx);

        if (eax >= 0x80000004) {

            for (int i = 0; i < 3; i++) {
                __cpuid(0x80000002 + i, eax, ebx, ecx, edx);
                *p++ = eax;
                *p++ = ebx;
                *p++ = ecx;
                *p++ = edx;
            }
        } else {
            __cpuid(0, eax, ebx, ecx, edx);
            p[0] = ebx;
            p[1] = edx;
            p[2] = ecx;

            p[12] = '\0';

            __cpuid(1, eax, ebx, ecx, edx);

            uint32_t base_model = (eax >> 4) & 0xF;
            uint32_t base_family = (eax >> 8) & 0xF;
            uint32_t ext_model = (eax >> 16) & 0xF;
            uint32_t ext_family = (eax >> 20) & 0xFF;

            uint32_t family = base_family;
            if (family == 0xF)
                family += ext_family;

            uint32_t model = base_model;
            if (base_family == 0x6 || base_family == 0xF)
                model |= (ext_model << 4);

            strfmt(cpu_name, "%s (Family %d Model %d)", cpu_name, family, model);
        }
        strfmt(buff, "CPU: %s\n", cpu_name);
        term_write(buff, COLOR_WHITE, COLOR_BLACK);
        strfmt(buff, "Memory: %d MB\n", ((heap_end - heap_start) >> 20) + 2);
        term_write(buff, COLOR_WHITE, COLOR_BLACK);
        strcpy(buff, "Uptime:");
        term_write(buff, COLOR_WHITE, COLOR_BLACK);
        if (uptime_hours > 0) {
            strfmt(buff, " %d hours", uptime_hours);
            term_write(buff, COLOR_WHITE, COLOR_BLACK);
        }
        if (uptime_minutes > 0) {
            strfmt(buff, " %d minutes", uptime_minutes);
            term_write(buff, COLOR_WHITE, COLOR_BLACK);
        }
        if (uptime_seconds > 0) {
            strfmt(buff, " %d seconds", uptime_seconds);
            term_write(buff, COLOR_WHITE, COLOR_BLACK);
        }
        term_write("\n", COLOR_BLACK, COLOR_BLACK);

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
