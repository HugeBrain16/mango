#ifndef KERNEL_H
#define KERNEL_H

#define unused(x) (void)(x)

void abort();
void panic(const char *msg);

#endif
