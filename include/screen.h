#ifndef SCREEN_H
#define SCREEN_H

#include <stdint.h>
#include "multiboot.h"

uint32_t *screen_buffer;
int screen_width;
int screen_height;
uint32_t screen_pitch;
float screen_scale;

extern void screen_init(multiboot_info_t *mbi);
extern void screen_init_back_buffer();
extern void screen_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, float scale);
extern void screen_clear(uint32_t color);
extern void screen_scroll(int lines, uint32_t color);
extern void screen_flush();

#endif
