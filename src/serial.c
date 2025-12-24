#include <stdint.h>
#include "serial.h"
#include "io.h"
#include "string.h"

void serial_init() {
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x80);
    outb(SERIAL_PORT + 0, 0x03);
    outb(SERIAL_PORT + 1, 0x00);
    outb(SERIAL_PORT + 3, 0x03);
    outb(SERIAL_PORT + 2, 0xC7);
    outb(SERIAL_PORT + 4, 0x0B);
}

void serial_putchr(char c) {
    while ((inb(SERIAL_PORT + 5) & 0x20) == 0);
    outb(SERIAL_PORT, c);
}

void serial_write(const char *str) {
    size_t length = strlen(str);

    for (size_t i = 0; i < length; i++) {
        serial_putchr(str[i]);
    }
}
