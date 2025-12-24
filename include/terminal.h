#include <stdint.h>
#include "list.h"

#define TERMINAL_BUFFER_SIZE 1024

list_t term_buffer;

void term_init();
void term_write(const char *msg, uint32_t fg_color, uint32_t bg_color);
