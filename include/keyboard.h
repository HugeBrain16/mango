#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

#define KEY_ESC 0x01
#define KEY_ALT 0x38
#define KEY_CTRL 0x1D
#define KEY_LSHIFT 0x2A
#define KEY_RSHIFT 0x36

#define KEY_ARROW_LEFT 0x4B
#define KEY_ARROW_RIGHT 0x4D
#define KEY_ARROW_UP 0x48
#define KEY_ARROW_DOWN 0x50

#define KEY_RELEASE 0x80

#define KEYBOARD_MODE_NONE 0
#define KEYBOARD_MODE_TERM 1
#define KEYBOARD_MODE_EDIT 2

int keyboard_shift;
int keyboard_mode;

char scancode_to_char(uint8_t scancode);
void keyboard_handle();

#endif
