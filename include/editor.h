#ifndef EDITOR_H
#define EDITOR_H

#include <stdint.h>
#include "color.h"
#include "file.h"

#define EDITOR_BG COLOR_DARKGRAY
#define EDITOR_FG COLOR_WHITE
#define EDITOR_STATUS_BG COLOR_LIGHTGRAY
#define EDITOR_STATUS_FG COLOR_WHITE

uint32_t edit_node;
char *edit_buffer;
size_t edit_pos;
size_t edit_cursor;
size_t edit_x;
size_t edit_y;

void edit_init(uint32_t file_sector);
void edit_draw_cursor();
void edit_write(const char *text, uint32_t fg_color, uint32_t bg_color);
void edit_handle_type(uint8_t scancode);

#endif
