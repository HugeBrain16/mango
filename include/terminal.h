#include <stdint.h>
#include "list.h"

#define TERM_BUFFER_SIZE 1024
#define TERM_INPUT_SIZE 1024

#define TERM_MODE_NONE 0
#define TERM_MODE_TYPE 1
#define TERM_MODE_EDIT 2

list_t term_buffer;
int term_input_cursor;
int term_input_pos;
int term_prompt;
char term_input[TERM_INPUT_SIZE];
int term_x;
int term_y;
int term_scale;
int term_mode;

void term_init();
void term_write(const char *msg, uint32_t fg_color, uint32_t bg_color);
void term_draw_cursor();
void term_handle_type(uint8_t scancode);
