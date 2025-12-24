#include <stdint.h>
#include "multiboot.h"

uint32_t *screen_buffer;
uint32_t screen_width;
uint32_t screen_height;
uint32_t screen_pitch;

void screen_init(multiboot_info_t *mbi);
void screen_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, int scale);
void screen_clear(uint32_t color);
void screen_scroll(int lines);
