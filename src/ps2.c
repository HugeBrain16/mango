#include "ps2.h"
#include "io.h"
#include "kernel.h"

void ps2_wait_read() {
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT));
}

void ps2_wait_write() {
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT);
}

void ps2_init() {
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_COMMAND_DISABLE_KEYBOARD);
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_COMMAND_DISABLE_MOUSE);

    inb(PS2_DATA_PORT);

    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_COMMAND_READ_CONFIG);
    ps2_wait_read();
    uint8_t config = inb(PS2_DATA_PORT);

    config |= PS2_CONFIG_KEYBOARD_IRQ | PS2_CONFIG_MOUSE_IRQ;

    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_COMMAND_WRITE_CONFIG);
    ps2_wait_write();
    outb(PS2_DATA_PORT, config);

    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_COMMAND_ENABLE_KEYBOARD);
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, PS2_COMMAND_ENABLE_MOUSE);
}
