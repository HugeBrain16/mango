#include <stdint.h>

#define KEY_ALT 0x38
#define KEY_CTRL 0x1D
#define KEY_LSHIFT 0x2A
#define KEY_RSHIFT 0x36

#define KEY_ARROW_LEFT 0x4B
#define KEY_ARROW_RIGHT 0x4D

#define KEY_RELEASE 0x80

int keyboard_shift;

char scancode_to_char(uint8_t scancode);
void keyboard_handle();
