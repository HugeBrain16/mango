#ifndef EDITOR_H
#define EDITOR_H

#define EDIT_MAX_LINE 128
#define EDIT_MAX_SIZE 128

#include <stdint.h>
#include "file.h"

file_node_t *edit_node;
char edit_buffer[EDIT_MAX_LINE][EDIT_MAX_SIZE];
char *line_buffer[EDIT_MAX_LINE];
int edit_line;
int edit_pos;
int edit_cursor;
int edit_x;
int edit_y;

void edit_init();
void edit_draw_cursor();
void edit_write(const char *text, uint32_t fg_color, uint32_t bg_color);
void edit_handle_type(uint8_t scancode);

#endif
