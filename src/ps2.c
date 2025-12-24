#include "ps2.h"
#include "io.h"

static void ps2_wait_read() {
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT));
}

static void ps2_wait_write() {
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT);
}

void ps2_init() {
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_COMMAND_DISABLE_KEYBOARD);
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_COMMAND_DISABLE_MOUSE);

    inb(PS2_DATA_PORT);

    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_COMMAND_ENABLE_KEYBOARD);
}

uint8_t ps2_read() {
    ps2_wait_read();
    return inb(PS2_DATA_PORT);
}

void ps2_write(uint8_t data) {
    ps2_wait_write();
    outb(PS2_DATA_PORT, data);
}

char ps2_get_char(uint8_t scancode) {
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

    if (sizeof(ascii) > scancode)
        return ascii[scancode];

    return 0;
}