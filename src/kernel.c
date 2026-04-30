#include <stdint.h>
#include "multiboot.h"
#include "serial.h"
#include "string.h"
#include "heap.h"
#include "screen.h"
#include "terminal.h"
#include "color.h"
#include "ps2.h"
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "pic.h"
#include "pit.h"
#include "file.h"
#include "keyboard.h"
#include "editor.h"
#include "ata.h"
#include "rtc.h"
#include "config.h"
#include "acpi.h"
#include "unit.h"
#include "cpu.h"
#include "script.h"
#include "paging.h"
#include "rand.h"
#include "pci.h"

uintptr_t __stack_chk_guard;

void log(const char *msg) {
    serial_write(msg);

    if (screen_buffer) {
        term_write(msg);
        screen_flush();
    }
}

__attribute__((noreturn))
void abort() {
    __asm__ volatile("cli");
    for (;;)
        __asm__ volatile("hlt");
    __builtin_unreachable();
}

__attribute__((noreturn))
void panic(const char *msg) {
    log(msg);
    abort();
    __builtin_unreachable();
}

__attribute__((noreturn))
void __stack_chk_fail() {
    panic("[ PANIC ] Stack smashing detected!\n");
    __builtin_unreachable();
}

__attribute__((noreturn))
void main(uint32_t magic, multiboot_info_t *mbi) {
    char buffer[64];

    serial_init();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
        panic("[ PANIC ] Invalid magic number!\n");
    if (!(mbi->flags & (1 << 12)))
        panic("[ PANIC ] No video!\n");

    screen_init(mbi);
    term_init();

    strfmt(buffer, "[ INFO ] Screen: %dx%d\n", screen_width, screen_height);
    log(buffer);

    gdt_init();
    log("[ INFO ] GDT OK\n");
    idt_init();
    log("[ INFO ] IDT OK\n");
    pages_init();
    log("[ INFO ] Pages OK\n");
    rtc_init();
    log("[ INFO ] RTC OK\n");
    ps2_init();
    log("[ INFO ] PS/2 OK\n");

    cpu_init();
    if (strlen(cpu_name) > 0)
        strfmt(buffer, "[ INFO ] CPU: %s\n", cpu_name);
    else
        strfmt(buffer, "[ INFO ] CPU: Unknown\n");
    log(buffer);

    heap_init(mbi);
    strfmt(buffer, "[ INFO ] Heap: 0x%x - 0x%x\n", heap_start, heap_end);
    log(buffer);

    for (int x = 0; x < PCI_MAX_BUS; x++) {
        for (int y = 0; y < PCI_MAX_DEV; y++) {
            pci_device_t dev = {0};

            char idstr[9];
            if (pci_get_device(&dev, x, y)) {
                strfmt(buffer, "[ INFO ] (PCI) Bus:%d Device:%d ", x, y);
                log(buffer);

                if (dev.revision) {
                    strfmt(buffer, "Rev:%d ", dev.revision);
                    log(buffer);
                }

                strhex(idstr, (uint32_t)dev.vendor_id);
                log(strsub(idstr, 4));

                log(":");

                strhex(idstr, (uint32_t)dev.device_id);
                log(strsub(idstr, 4));

                log("\n");
            }
        }
    }

    screen_init_back_buffer();
    log("[ INFO ] Initialized screen back buffer.\n");

    char total_mem[16];
    unit_get_size(heap_end - heap_start + (2 << 20), total_mem);
    strfmt(buffer, "[ INFO ] Memory: %s\n", total_mem);
    log(buffer);

    log("[ INFO ] Checking ATA port PRIMARY MASTER for drive...\n");
    file_init(ATA_PRIMARY, ATA_MASTER);

    uint8_t status = ata_status(file_port);
    if (status == 0x00 || status == 0xFF) {
        file_drive_status = FILE_DRIVE_ABSENT;
        strfmt(buffer, "[ WARNING ] No primary drive. (status: %x)\n", status);
        log(buffer);
    }

    if (file_drive_status != FILE_DRIVE_ABSENT) {
        uint8_t ata_id[512];
        ata_identify(file_port, ata_id);

        uint16_t *w = (uint16_t*) ata_id;
        if (w[0] & (1 << 15)) {
            file_drive_status = FILE_DRIVE_INCOMPATIBLE;
            log("[ WARNING ] Drive is not compatible.\n");
        }
    }

    if (file_drive_status == FILE_DRIVE_OK)
        log("[ INFO ] Drive OK\n");

    pit_set_frequency(250);
    strfmt(buffer, "[ INFO ] PIT frequency: %d\n", pit_hz);
    log(buffer);

    pic_unmask(0);
    log("[ INFO ] PIT OK\n");
    pic_unmask(1);
    log("[ INFO ] PS/2 Keyboard OK\n");

    acpi_init();

    log("[ INFO ] Seeding random...\n");
    uint64_t dt = datetime_packed();
    srand((uint32_t)(dt >> 32) ^ (uint32_t)dt ^ pit_ticks ^ rtc_ticks);

    log("[ INFO ] Initialization complete!\n");
    term_init();
    term_load_config();
    term_update_path();
    screen_clear(term_bg);
    keyboard_mode = KEYBOARD_MODE_TERM;

    if (file_drive_status == FILE_DRIVE_OK && file_path_isfile("/system/init.sc"))
        script_run("/system/init.sc", 0, NULL);

    char *scale_config = config_get("/system/config/screen.cfg", "scale");
    if (scale_config)
        screen_scale = doublestr(scale_config);

    if (!config_has("/system/config/system.cfg", "disable_welcome_message"))
        term_write2("Welcome to Mango!\n", COLOR_YELLOW, term_bg);
    term_draw_prompt();

    for (;;)
        __asm__ volatile("hlt");
}
