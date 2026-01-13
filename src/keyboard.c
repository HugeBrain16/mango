#include "keyboard.h"
#include "io.h"
#include "ps2.h"
#include "terminal.h"
#include "editor.h"

int keyboard_shift = 0;
int keyboard_mode = KEYBOARD_MODE_NONE;

static const char ascii[] = {
    0, 0, '1', '2', '3', '4', '5', '6',
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i',
    'o', 'p', '[', ']', '\n', 0,  'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';',
    '\'', '`', 0,  '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', 0,  '*',
    0, ' '
};

static const char ascii_shift[] = {
    0, 0, '!', '@', '#', '$', '%', '^',
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I',
    'O', 'P', '{', '}', '\n', 0,  'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':',
    '"', '~', 0,  '|',  'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', 0,  '*',
    0, ' '
};

char scancode_to_char(uint8_t scancode) {
    if (sizeof(ascii) > scancode)
        return keyboard_shift ? ascii_shift[scancode] : ascii[scancode];

    return 0;
}

void keyboard_handle() {
    uint8_t scancode = inb(PS2_DATA_PORT);

    if (scancode & KEY_RELEASE) {
        uint8_t key = scancode & 0x7F;

        if (key == KEY_LSHIFT || key == KEY_RSHIFT)
            keyboard_shift = 0;
    }

    if (scancode == KEY_LSHIFT || scancode == KEY_RSHIFT)
        keyboard_shift = 1;

    if (keyboard_mode == KEYBOARD_MODE_TERM)
        term_handle_type(scancode);
    else if (keyboard_mode == KEYBOARD_MODE_EDIT)
        edit_handle_type(scancode);
}
