#ifndef EDITOR_H
#define EDITOR_H

#include <stdint.h>
#include "color.h"
#include "file.h"

#define EDITOR_CURSOR_BLINK 500
#define EDITOR_BG COLOR_DARKGRAY
#define EDITOR_FG COLOR_WHITE
#define EDITOR_STATUS_BG COLOR_LIGHTGRAY
#define EDITOR_STATUS_FG COLOR_WHITE

extern uint32_t edit_node;
extern char *edit_buffer;
extern size_t edit_pos;
extern size_t edit_cursor;
extern int edit_x;
extern int edit_y;

extern void edit_init(uint32_t file_sector);
extern void edit_draw_cursor();
extern void edit_write(const char *text, uint32_t fg_color, uint32_t bg_color);
extern void edit_handle_type(uint8_t scancode);

#endif
