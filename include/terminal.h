#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stddef.h>

#define TERM_INPUT_SIZE 128

int term_input_cursor;
int term_input_pos;
int term_prompt;
char term_input[TERM_INPUT_SIZE];
char *term_input_buffer;
int term_x;
int term_y;

void term_init();
void term_write(const char *msg, uint32_t fg_color, uint32_t bg_color);
void term_draw_cursor();
void term_handle_type(uint8_t scancode);
void term_get_input(const char* prompt, char *buffer, size_t size, uint32_t fg_color, uint32_t bg_color);

#endif
