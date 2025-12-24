#include "terminal.h"
#include "string.h"
#include "screen.h"
#include "font.h"

int term_x = 0;
int term_y = 0;
int term_scale = 1;

void term_init() {
    list_init(&term_buffer);
}

void term_write(const char *msg, uint32_t fg_color, uint32_t bg_color) {
    for (size_t i = 0; i < strlen(msg); i++) {
        char c = msg[i];

        if (c != '\n')
            screen_draw_char(term_x, term_y, c, fg_color, bg_color, term_scale);
        
        term_x += FONT_WIDTH * term_scale;
        if (c == '\n') {
            term_x = 0;
            term_y += FONT_HEIGHT * term_scale;
        }
    }
    list_push(&term_buffer, (char *) msg);

    if (term_buffer.size >= TERMINAL_BUFFER_SIZE)
        list_pop(&term_buffer);
}
