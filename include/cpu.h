#ifndef CPU_H
#define CPU_H

#include <stdint.h>

char cpu_name[64];
char cpu_vendor[16];
uint32_t cpu_family;
uint32_t cpu_model;

void cpu_init();

#endif