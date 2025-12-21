#define SERIAL_ENABLE_ALIAS

#include <stdint.h>
#include "multiboot.h"
#include "serial.h"
#include "string.h"


void main(uint32_t magic, multiboot_info_t *mbi) {
    serial_init();
    serial_writeln("Mango kernel");

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        serial_writeln("Invalid magic number");
        return;
    }

    if (!(mbi->flags & (1 << 12))) {
        serial_writeln("No video");
        return;
    }
}
