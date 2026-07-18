#ifndef KERNEL_H
#define KERNEL_H

#include "string.h"

#define unused(x) (void)(x)

extern char early_boot[128];
extern int boot_logging;
extern string_t *boot_log;

extern void log(const char *msg);
extern void abort();
extern void panic(const char *msg);

#endif
