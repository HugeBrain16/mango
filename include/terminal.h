#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stddef.h>
#include "color.h"

#define TERM_CURSOR_BLINK 500
#define TERM_INPUT_SIZE 128
#define TERM_MAX_HISTORY 32

#define TERM_COLOR_FG COLOR_WHITE
#define TERM_COLOR_BG COLOR_BLACK

int term_input_cursor;
int term_input_pos;
int term_prompt;
char term_input[TERM_INPUT_SIZE];
char *term_input_buffer;
int term_x;
int term_y;
int term_fg;
int term_bg;

extern void term_init();
extern void term_write(const char *msg);
extern void term_write2(const char *msg, uint32_t fg_color, uint32_t bg_color);
extern void term_draw_cursor();
extern void term_handle_type(uint8_t scancode);
extern void term_get_input(const char* prompt, char *buffer, size_t size);
extern void term_draw_prompt();
extern void term_load_config();
extern void term_update_path();

#endif
