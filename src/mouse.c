#include "mouse.h"
#include "kernel.h"
#include "string.h"
#include "io.h"
#include "ps2.h"
#include "pic.h"
#include "idt.h"
#include "screen.h"
#include "color.h"
#include "config.h"
#include "fio.h"
#include "keyboard.h"
#include "heap.h"

int mouse_rate = 100;
int mouse_x = 0;
int mouse_y = 0;
int mouse_middle = 0;
int mouse_right = 0;
int mouse_left = 0;
uint32_t mouse_color = COLOR_WHITE;
image_t *mouse_cursor = NULL;

static int mouse_update = 0;
static int mouse_last_x = -1;
static int mouse_last_y = -1;
static uint8_t mouse_packet[3] = {0};
static uint32_t mouse_old[16*16] = {0};

static int mouse_command(uint8_t command) {
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, MOUSE_ADDRESS_ME);
    ps2_wait_write();
    outb(PS2_DATA_PORT, command);

    ps2_wait_read();
    return inb(PS2_DATA_PORT) == MOUSE_ACK;
}

static int mouse_data(uint8_t data) {
    ps2_wait_write();
    outb(PS2_COMMAND_PORT, MOUSE_ADDRESS_ME);
    ps2_wait_write();
    outb(PS2_DATA_PORT, data);

    ps2_wait_read();
    return inb(PS2_DATA_PORT) == MOUSE_ACK;
}

int mouse_set_rate(int rate) {
    char msg[64];

    switch (rate) {
        case 10:
        case 20:
        case 40:
        case 60:
        case 80:
        case 100:
        case 200:
            if (!mouse_command(MOUSE_SET_RATE)) return 0;
            if (!mouse_data(rate)) return 0;
            mouse_rate = rate;
            strfmt(msg, "[ INFO ] Mouse rate: %d\n", rate);
            log(msg);
            return 1;
        default:
            strfmt(msg, "[ ERROR ] Invalid mouse rate value: %d\n", rate);
            log(msg);
            return 0;
    }
}

int mouse_load_cursor() {
    fio_t *file = NULL;

    char *cursor = config_get("/system/config/desktop.cfg", "cursor");
    if (cursor) {
        file = fio_open(cursor, 'r');
        heap_free(cursor);
    } else
        file = fio_open("/system/assets/cursor.png", 'r');

    if (file) {
        if (mouse_cursor) {
            image_free(mouse_cursor);
            mouse_cursor = NULL;
        }

        size_t size = file->node->size;
        char *raw = heap_alloc(size);
        if (fio_read(file, raw, size)) {
            mouse_cursor = image_png(raw, size);
            heap_free(raw);
        }
        fio_close(file);
    }

    return mouse_cursor != NULL;
}

void mouse_init() {
    if (mouse_command(MOUSE_ENABLE_REPORTING))
        log("[ INFO ] Mouse data reporting enabled\n");
    else
        log("[ ERROR ] Mouse failed to enable data reporting\n");

    mouse_set_rate(mouse_rate);
    pic_unmask(12);
}

void mouse_handle() {
    uint8_t byte = inb(PS2_DATA_PORT);
    if (keyboard_mode != KEYBOARD_MODE_DESKTOP) return;
    if (mouse_update == 0 && !(byte & 0x08)) return;

    mouse_packet[mouse_update++] = byte;
    if (mouse_update < 3) return;

    mouse_update = 0;

    uint8_t meta = mouse_packet[0];
    int xaxis = mouse_packet[1];
    int yaxis = mouse_packet[2];

    if (meta & 0x10) xaxis |= 0xFFFFFF00;
    if (meta & 0x20) yaxis |= 0xFFFFFF00;

    mouse_left = (meta >> 0) & 1;
    mouse_right = (meta >> 1) & 1;
    mouse_middle = (meta >> 2) & 1;

    int x = mouse_x + xaxis;
    int y = mouse_y - yaxis;

    if (x < 0) x = 0;
    else if (x >= screen_width) x = screen_width - 1;

    if (y < 0) y = 0;
    else if (y >= screen_height) y = screen_height - 1;

    mouse_x = x;
    mouse_y = y;
}

void mouse_draw() {
    if (mouse_last_x >= 0 && mouse_last_y >= 0) {
        size_t idx = 0;
        for (int y = 0; y < 16; y++) {
            for (int x = 0; x < 16; x++)
                screen_draw_pixel(mouse_last_x + x, mouse_last_y + y, mouse_old[idx++], 1);
        }
    }

    mouse_last_x = mouse_x;
    mouse_last_y = mouse_y;

    size_t idx = 0;
    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++)
            screen_get_pixel(mouse_last_x + x, mouse_last_y + y, &mouse_old[idx++], 1);
    }

    if (mouse_cursor)
        screen_draw_rgba(mouse_cursor->data, mouse_cursor->size,
            mouse_last_x, mouse_last_y, mouse_cursor->width, mouse_cursor->height, 1);
    else
        screen_draw_pixel(mouse_last_x, mouse_last_y, mouse_color, 1);
}