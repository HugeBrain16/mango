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
