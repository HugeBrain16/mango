#ifndef CPU_H
#define CPU_H

#include <stdint.h>

extern char cpu_name[64];
extern char cpu_vendor[16];
extern uint32_t cpu_family;
extern uint32_t cpu_model;

extern void cpu_init();

#endif