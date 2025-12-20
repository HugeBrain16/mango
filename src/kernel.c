#define SERIAL_ENABLE_ALIAS

#include <stdint.h>
#include "multiboot.h"
#include "serial.h"
#include "string.h"


void main(uint32_t magic, multiboot_info_t *mbi) {
    serial_init();
    serial_writeln("Mango kernel");

    if (!(mbi->flags & (1 << 12))) {
        serial_write("No video");
        return;
    }
    
    char buff[16];
    strint(buff, mbi->mem_lower);
    serial_writeln(buff);
}
