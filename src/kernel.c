#include <stdint.h>
#include "kernel.h"
#include "multiboot.h"
#include "serial.h"
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
#include "sound.h"
#include "command.h"
#include "fio.h"

char early_boot[128] = {0};
int boot_logging = 1;
string_t *boot_log = NULL;
uintptr_t __stack_chk_guard;

static void setup_log() {
    if (!file_path_isfile("/system/system.log")) {
        if (!file_path_isfolder("/system")) command_handle("newfolder /system", 0);
        command_handle("newfile /system/system.log", 0);
    }
}

void log(const char *msg) {
    serial_write(msg);

    if (screen_buffer) {
        term_write(msg);
        screen_flush();
    }

    if (file_is_ready() && !boot_logging) {
        setup_log();

        fio_t *syslog = fio_open("/system/system.log", 'a');
        fio_write(syslog, msg, strlen(msg));
        fio_close(syslog);
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
    char *msg;
    char buffer[64];

    serial_init();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        msg = "[ PANIC ] Invalid magic number!\n";
        strcat(early_boot, msg);
        panic(msg);
    }
    if (!(mbi->flags & (1 << 12))) {
        msg = "[ PANIC ] No video!\n";
        strcat(early_boot, msg);
        panic(msg);
    }

    screen_init(mbi);
    term_init();

    pages_init();
    msg = "[ INFO ] Pages OK\n";
    strcat(early_boot, msg);
    log(msg);

    heap_init(mbi);
    strfmt(buffer, "[ INFO ] Heap: 0x%x - 0x%x\n", heap_start, heap_end);
    strcat(early_boot, buffer);
    log(buffer);

    boot_log = string_from(early_boot);

    strfmt(buffer, "[ INFO ] Screen: %dx%d\n", screen_width, screen_height);
    string_puts(boot_log, buffer);
    log(buffer);

    gdt_init();
    msg = "[ INFO ] GDT OK\n";
    string_puts(boot_log, msg);
    log(msg);
    idt_init();
    msg = "[ INFO ] IDT OK\n";
    string_puts(boot_log, msg);
    log(msg);

    rtc_init();
    msg = "[ INFO ] RTC OK\n";
    string_puts(boot_log, msg);
    log(msg);
    ps2_init();
    msg = "[ INFO ] PS/2 OK\n";
    string_puts(boot_log, msg);
    log(msg);

    cpu_init();
    if (strlen(cpu_name) > 0)
        strfmt(buffer, "[ INFO ] CPU: %s\n", cpu_name);
    else
        strfmt(buffer, "[ INFO ] CPU: Unknown\n");
    string_puts(boot_log, buffer);
    log(buffer);

    for (int x = 0; x < PCI_MAX_BUS; x++) {
        for (int y = 0; y < PCI_MAX_DEV; y++) {
            pci_device_t dev = {0};

            char idstr[9];
            if (pci_get_device(&dev, x, y)) {
                strfmt(buffer, "[ INFO ] (PCI) Bus:%d Device:%d ", x, y);
                string_puts(boot_log, buffer);
                log(buffer);

                if (dev.revision) {
                    strfmt(buffer, "Rev:%d ", dev.revision);
                    string_puts(boot_log, buffer);
                    log(buffer);
                }

                strhex(idstr, (uint32_t)dev.vendor_id);
                msg = strsub(idstr, 4);
                string_puts(boot_log, msg);
                log(msg);

                string_putc(boot_log, ':');
                log(":");

                strhex(idstr, (uint32_t)dev.device_id);
                msg = strsub(idstr, 4);
                string_puts(boot_log, msg);
                log(msg);

                string_putc(boot_log, '\n');
                log("\n");
            }
        }
    }

    screen_init_back_buffer();
    msg = "[ INFO ] Initialized screen back buffer\n";
    string_puts(boot_log, msg);
    log(msg);

    char total_mem[16];
    unit_get_size(heap_end - heap_start + (2 << 20), total_mem);
    strfmt(buffer, "[ INFO ] Memory: %s\n", total_mem);
    string_puts(boot_log, buffer);
    log(buffer);

    file_init_by_slot(1);

    pit_set_frequency(250);
    strfmt(buffer, "[ INFO ] PIT frequency: %d\n", pit_hz);
    string_puts(boot_log, buffer);
    log(buffer);

    pic_unmask(0);
    msg = "[ INFO ] PIT OK\n";
    string_puts(boot_log, msg);
    log(msg);
    pic_unmask(1);
    msg = "[ INFO ] PS/2 Keyboard OK\n";
    string_puts(boot_log, msg);
    log(msg);

    acpi_init();

    msg = "[ INFO ] Seeding random...\n";
    string_puts(boot_log, msg);
    log(msg);
    uint64_t dt = datetime_packed();
    srand((uint32_t)(dt >> 32) ^ (uint32_t)dt ^ pit_ticks ^ rtc_ticks);

    msg = "[ INFO ] Initializing sound...\n";
    string_puts(boot_log, msg);
    log(msg);
    if(sound_init()) {
        msg = "[ INFO ] Sound OK\n";
        string_puts(boot_log, msg);
        log(msg);
    } else {
        msg = "[ WARNING ] Sound initialization failed!\n";
        string_puts(boot_log, msg);
        log(msg);
    }

    msg = "[ INFO ] Initialization complete!\n";
    string_puts(boot_log, msg);
    log(msg);

    setup_log();
    fio_t *syslog = fio_open("/system/system.log", 'w');
    fio_write(syslog, boot_log->value, boot_log->size);
    fio_close(syslog);
    string_free(boot_log);
    boot_logging = 0;

    term_init();
    term_load_config();
    term_update_path();
    screen_clear(term_bg);
    keyboard_mode = KEYBOARD_MODE_TERM;

    if (file_is_ready() && file_path_isfile("/system/init.sc"))
        script_run("/system/init.sc", 0, NULL);

    char *scale_config = config_get("/system/config/screen.cfg", "scale");
    if (scale_config) {
        screen_scale = doublestr(scale_config);
        heap_free(scale_config);
    }

    if (!config_has("/system/config/system.cfg", "disable_welcome_message"))
        term_write2("Welcome to Mango!\n", COLOR_YELLOW, term_bg);
    term_draw_prompt();

    for (;;)
        __asm__ volatile("hlt");
}
