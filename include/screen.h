#ifndef SCREEN_H
#define SCREEN_H

#include <stddef.h>
#include <stdint.h>
#include "multiboot.h"

extern uint32_t *screen_buffer;
extern uint32_t *back_buffer;
extern size_t back_buffer_size;

extern int screen_width;
extern int screen_height;
extern uint32_t screen_pitch;
extern float screen_scale;

extern void screen_init(multiboot_info_t *mbi);
extern void screen_init_back_buffer();
extern int screen_get_pixel(int x, int y, uint32_t *color, int direct);
extern void screen_draw_pixel(int x, int y, uint32_t color, int direct);
extern void screen_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, float scale);
extern void screen_draw_char2(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, float scale, int direct);
extern void screen_draw_rgba(const void *data, size_t size, int x, int y, int width, int height, int direct);
extern void screen_clear(uint32_t color);
extern void screen_scroll(int lines, uint32_t color);
extern void screen_flush();

#endif
