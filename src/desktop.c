#include "desktop.h"
#include "kernel.h"
#include "heap.h"
#include "screen.h"
#include "color.h"
#include "keyboard.h"
#include "terminal.h"
#include "fio.h"
#include "config.h"
#include "media.h"
#include "mouse.h"
#include "font.h"
#include "modules.h"
#include "pit.h"
#include "unit.h"
#include "editor.h"

int desktop_active = 0;

static image_t *desktop_wall = NULL;
static uint32_t desktop_clear = COLOR_BLUE;
static uint32_t desktop_status_update = 1000;
static uint32_t desktop_status_last = 0;
static uint32_t desktop_status_fg = COLOR_ORANGE;
static uint32_t desktop_status_bg = COLOR_WHITE;
static list_t *desktop_icons = NULL;
static uint32_t desktop_icon_text_fg = COLOR_BLACK;
static uint32_t desktop_icon_text_bg = COLOR_WHITE;

static void clear(int x, int y, uint32_t color) {
    int draw_x = x;
    while (draw_x < screen_width) {
        screen_draw_char(draw_x, y, ' ', color, color, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
    }
}

static void desktop_draw_wall() {
	if (!desktop_wall) return;
    screen_draw_rgba(desktop_wall->data,
    	desktop_wall->size, 0, 0, desktop_wall->width, desktop_wall->height, 0);
}

static desktop_icon_t *desktop_create_icon(const char *path, const char *name) {
	fio_t *file = fio_open(path, 'r');
	if (file) {
		size_t size = file->node->size;
		char *data = heap_alloc(size);
		fio_read(file, data, size);
		fio_close(file);

		desktop_icon_t *icon = heap_alloc(sizeof(desktop_icon_t));
		icon->image = image_png(data, size);

		size_t length = strlen(name) + 1;
		icon->name = heap_alloc(length);
		strcpy(icon->name, name);
		heap_free(data);
		return icon;
	}

	return NULL;
}

static void desktop_free_icon(desktop_icon_t *icon) {
	if (icon) {
		heap_free(icon->name);
		image_free(icon->image);
		heap_free(icon);
	}
}

static void desktop_draw_statusbar() {
	uint32_t ticks = pit_ticks;
	if (ticks - desktop_status_last < ms_to_ticks(desktop_status_update)) return;
	desktop_status_last = ticks;

    int draw_x = 0;
    int draw_y = screen_height - ((FONT_WIDTH * screen_scale) * 2);

    clear(draw_x, draw_y, desktop_status_bg);

    char time[8];
    char date[16];
    module_time(time, 0);
    module_date(date, 0);

    size_t used;
    size_t usable;
    size_t free;
    heap_stat(&used, &usable, &free, NULL);

    char status_left[128];
    char status_right[128];
    strfmt(status_left,"Mango OS b%d", BUILD_NUMBER);
    strfmt(status_right, "MEM: %d/%dKB | %s %s",
    	used >> 10, (free + usable) >> 10, date, time);

    draw_x = 0;
    for (const char *p = status_left; *p != '\0'; p++) {
        screen_draw_char(draw_x, draw_y, *p, desktop_status_fg, COLOR_TRANSPARENT, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
    }

    draw_x = screen_width - (FONT_WIDTH * screen_scale);
    for (int i = strlen(status_right) - 1; i >= 0 && status_right[i] != '\0'; i--) {
    	screen_draw_char(draw_x, draw_y, status_right[i], desktop_status_fg, COLOR_TRANSPARENT, screen_scale);
        draw_x -= FONT_WIDTH * screen_scale;
    }

    screen_flush();
}

static void desktop_draw_icons() {
	int draw_x = 0;
	int draw_y = 0;
	int spacing = 16;

	for (size_t i = 0; i < desktop_icons->size; i++) {
		int x = draw_x;
		int y = draw_y;

		desktop_icon_t *icon = (desktop_icon_t*)list_get(desktop_icons, i);
		if (!icon || !icon->image) continue;

		icon->x = x + spacing;
		icon->y = y + spacing;

		screen_draw_rgba(
			icon->image->data,
			icon->image->size,
			icon->x, icon->y,
			icon->image->width,
			icon->image->height,
			0);
		x += icon->image->width * 2;

		for (int i = 0; icon->name[i] != '\0'; i++) {
			screen_draw_char(
				(draw_x + 8) + (FONT_WIDTH * i),
				draw_y + (FONT_HEIGHT + spacing) * 1.5,
				icon->name[i],
				desktop_icon_text_fg,
				desktop_icon_text_bg,
				screen_scale);
		}

		if (x >= (int)(screen_width - (icon->image->width + spacing))) {
			x = 0;
			y += icon->image->height + spacing;
		}

		draw_x = x;
		draw_y = y;
	}

	screen_flush();
}

static void desktop_handle_quit() {
	if (desktop_wall) {
		image_free(desktop_wall);
		desktop_wall = NULL;
	}
	if (mouse_cursor) {
		image_free(mouse_cursor);
		mouse_cursor = NULL;
	}
	if (desktop_icons) {
		while (desktop_icons->size > 0)
			desktop_free_icon((desktop_icon_t*)list_pop(desktop_icons));
		list_free(desktop_icons);
	}

	desktop_active = 0;
	term_init(TERM_DRAW_DEFAULT);
}

static void desktop_load_config() {
	fio_t *file = NULL;

	char *wall = config_get("/system/config/desktop.cfg", "wallpaper");
	if (wall) {
		file = fio_open(wall, 'r');
		heap_free(wall);
	} else
		file = fio_open("/system/assets/wallpaper.png", 'r');

	if (file) {
		if (desktop_wall) {
			image_free(desktop_wall);
			desktop_wall = NULL;
		}

		size_t size = file->node->size;
		char *raw = heap_alloc(size);
		if (fio_read(file, raw, size)) {
			desktop_wall = image_png(raw, size);
			if (!desktop_wall)
		    	log("[ ERROR ] (DESKTOP) Failed to load wallpaper, decoding error.\n");
		    heap_free(raw);
		} else log("[ ERROR ] (DESKTOP) Failed to load wallpaper, can't read file.\n");
		fio_close(file);
	} else log("[ ERROR ] (DESKTOP) Failed to load wallpaper, file not found.\n");

	char *status_bg = config_get("/system/config/desktop.cfg", "status_bg");
	if (status_bg) {
		int col = color(status_bg);
		if (col != COLOR_INVALID)
			desktop_status_bg = col;
		else
			log("[ ERROR ] (DESKTOP) Invalid color name for status_bg\n");
		heap_free(status_bg);
	}
	char *status_fg = config_get("/system/config/desktop.cfg", "status_fg");
	if (status_fg) {
		int col = color(status_fg);
		if (col != COLOR_INVALID)
			desktop_status_fg = col;
		else
			log("[ ERROR ] (DESKTOP) Invalid color name for status_fg\n");
		heap_free(status_fg);
	}
	char *icon_text_bg = config_get("/system/config/desktop.cfg", "icon_text_bg");
	if (icon_text_bg) {
		int col = color(icon_text_bg);
		if (col != COLOR_INVALID)
			desktop_icon_text_bg = col;
		else
			log("[ ERROR ] (DESKTOP) Invalid color name for icon_text_bg\n");
		heap_free(icon_text_bg);
	}
	char *icon_text_fg = config_get("/system/config/desktop.cfg", "icon_text_fg");
	if (icon_text_fg) {
		int col = color(icon_text_fg);
		if (col != COLOR_INVALID)
			desktop_icon_text_fg = col;
		else
			log("[ ERROR ] (DESKTOP) Invalid color name for icon_text_fg\n");
		heap_free(icon_text_fg);
	}
}

void desktop_init() {
	desktop_active = 1;
	desktop_load_config();

	screen_clear(desktop_clear);
	desktop_draw_wall();
	mouse_load_cursor();
	keyboard_mode = KEYBOARD_MODE_DESKTOP;

	desktop_icons = heap_alloc(sizeof(list_t));
	list_init(desktop_icons);

	list_push(desktop_icons, desktop_create_icon("/system/assets/editor.png", "Editor"));
	list_push(desktop_icons, desktop_create_icon("/system/assets/terminal.png", "Terminal"));
	desktop_draw_icons();
}

void desktop_handle_type(uint8_t scancode) {
	if (keyboard_ctrl) {
        char c = scancode_to_char(scancode);

        if (!keyboard_shift) {
            if (c == 'q') return desktop_handle_quit();
            else if (c == 'r') {
            	log("[ INFO ] (DESKTOP) Restarted.\n");
            	desktop_handle_quit();
            	desktop_init();
            	return;
            }
        }
    }
}

void desktop_update() {
	desktop_draw_statusbar();
	mouse_draw();
	for (size_t i = 0; i < desktop_icons->size; i++) {
		desktop_icon_t *icon = (desktop_icon_t*)list_get(desktop_icons, i);
		if (!icon || !icon->image) continue;

		if (mouse_x > icon->x && mouse_x < icon->x + (int)icon->image->width
			&& mouse_y > icon->y && mouse_y < icon->y + (int)icon->image->height + FONT_HEIGHT) {
			if (!mouse_right && mouse_left && !mouse_middle) {
				if (!strcmp(icon->name, "Editor")) {
					edit_init(0);
				} else if (!strcmp(icon->name, "Terminal")) {
					term_init(TERM_DRAW_DEFAULT);
				}
				mouse_left = 0;
			}
		}
	}
}