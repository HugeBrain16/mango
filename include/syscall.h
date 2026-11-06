#ifndef SYSCALL_H
#define SYSCALL_H

#include "idt.h"

void syscall_handle(int_frame_t *frame);

#endif
