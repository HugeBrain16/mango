#include <stdint.h>
#include "multiboot.h"
#include "serial.h"

void main(multiboot_info_t *mbi) {
    serial_init();
    serial_write("We're in kernel!");
}
