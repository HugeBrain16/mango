#include <cpuid.h>
#include "cpu.h"
#include "string.h"

char cpu_name[64] = {0};
char cpu_vendor[16] = {0};
uint32_t cpu_family = 0;
uint32_t cpu_model = 0;

void cpu_init() {
    uint32_t eax, ebx, ecx, edx;

    __cpuid(0, eax, ebx, ecx, edx);
    memcpy(cpu_vendor + 0, &ebx, 4);
    memcpy(cpu_vendor + 4, &edx, 4);
    memcpy(cpu_vendor + 8, &ecx, 4);
    cpu_vendor[12] = '\0';

    __cpuid(0x80000000, eax, ebx, ecx, edx);
    if (eax >= 0x80000004) {
        uint32_t *p = (uint32_t*)cpu_name;
        for (int i = 0; i < 3; i++) {
            __cpuid(0x80000002 + i, eax, ebx, ecx, edx);
            *p++ = eax;
            *p++ = ebx;
            *p++ = ecx;
            *p++ = edx;
        }
        cpu_name[48] = '\0';
    } else {
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

        cpu_family = family;
        cpu_model = model;

        strfmt(cpu_name, "%s (Family %d Model %d)", cpu_vendor, family, model);
    }
}