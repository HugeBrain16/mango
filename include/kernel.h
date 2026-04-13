#ifndef KERNEL_H
#define KERNEL_H

#define unused(x) (void)(x)

extern void log(const char *msg);
extern void abort();
extern void panic(const char *msg);

#endif
