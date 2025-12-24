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

void ps2_init();
uint8_t ps2_read();
void ps2_write(uint8_t data);
char ps2_get_char(uint8_t scancode);