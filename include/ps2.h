#ifndef PS2_H
#define PS2_H

#include <stdint.h>

#define PS2_DATA_PORT 0x60
#define PS2_STATUS_PORT 0x64
#define PS2_COMMAND_PORT 0x64

#define PS2_STATUS_OUTPUT (1 << 0)
#define PS2_STATUS_INPUT (1 << 1)

#define PS2_COMMAND_DISABLE_KEYBOARD 0xAD
#define PS2_COMMAND_ENABLE_KEYBOARD 0xAE
#define PS2_COMMAND_DISABLE_MOUSE 0xA7
#define PS2_COMMAND_ENABLE_MOUSE 0xA8
#define PS2_COMMAND_READ_CONFIG 0x20
#define PS2_COMMAND_WRITE_CONFIG 0x60

#define PS2_CONFIG_KEYBOARD_IRQ (1 << 0)
#define PS2_CONFIG_MOUSE_IRQ (1 << 1)

extern void ps2_wait_write();
extern void ps2_wait_read();
extern void ps2_init();

#endif
