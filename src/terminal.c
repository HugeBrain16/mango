#include "terminal.h"
#include "string.h"
#include "screen.h"
#include "font.h"
#include "pit.h"
#include "color.h"
#include "keyboard.h"
#include "command.h"

int term_x = 0;
int term_y = 0;
int term_scale = 1;
int term_mode = TERM_MODE_NONE;
int term_cursor_visible = 1;

static uint32_t cursor_ticks = 0;
static int cursor_visible = 0;

void term_init() {
    list_init(&term_buffer);
    term_input_cursor = 0;
    term_input_pos = 0;
}

static void term_clear_cursor() {
    if (term_input_pos < term_input_cursor)
        screen_draw_char(term_x, term_y, term_input[term_input_pos], COLOR_WHITE, COLOR_BLACK, term_scale);
    else
        screen_draw_char(term_x, term_y, ' ', COLOR_WHITE, COLOR_BLACK, term_scale);
}

void term_draw_cursor() {
    if (pit_ticks - cursor_ticks >= 50) {
        cursor_visible = !cursor_visible;
        cursor_ticks = pit_ticks;

        term_clear_cursor();

        if (cursor_visible)
            screen_draw_char(term_x, term_y, '_', COLOR_WHITE, COLOR_TRANSPARENT, term_scale);
    }
}

static void term_redraw_cursor(int hide) {
    cursor_visible = hide;
    cursor_ticks = 0;
    term_draw_cursor();
}

void term_write(const char *msg, uint32_t fg_color, uint32_t bg_color) {
    term_redraw_cursor(1);

    for (size_t i = 0; i < strlen(msg); i++) {
        char c = msg[i];

        if (c != '\n')
            screen_draw_char(term_x, term_y, c, fg_color, bg_color, term_scale); 
        if (c != '\b')
            term_x += FONT_WIDTH * term_scale;
        if (c == '\n') {
            term_x = 0;
            term_y += FONT_HEIGHT * term_scale;

            int max_y = screen_height - (FONT_HEIGHT * term_scale);
            if (term_y > max_y) {
                screen_scroll(FONT_HEIGHT * term_scale);
                term_y = max_y;
            }
        }
    }
    list_push(&term_buffer, (char *) msg);

    if (term_buffer.size >= TERM_BUFFER_SIZE)
        list_pop(&term_buffer);
}

static void term_handle_backspace() {
    if (term_input_pos == 0) return;

    term_clear_cursor();

    for (int i = term_input_pos - 1; i < term_input_cursor - 1; i++) {
        term_input[i] = term_input[i + 1];
    }
    term_input[--term_input_cursor] = '\0';
    term_input_pos--;

    int saved_x = term_x - FONT_WIDTH * term_scale;
    term_x = saved_x;

    for (int i = term_input_pos; i < term_input_cursor; i++) {
        screen_draw_char(term_x, term_y, term_input[i], COLOR_WHITE, COLOR_BLACK, term_scale);
        term_x += FONT_WIDTH * term_scale;
    }

    screen_draw_char(term_x, term_y, ' ', COLOR_WHITE, COLOR_BLACK, term_scale);
    term_x = saved_x;

    term_redraw_cursor(0);
}

static void term_handle_left() {
    if (term_input_pos == 0) return;

    term_clear_cursor();
    term_x -= FONT_WIDTH * term_scale;
    term_input_pos--;
    term_redraw_cursor(0);
}

static void term_handle_right() {
    if (term_input_pos == term_input_cursor) return;

    term_clear_cursor();
    term_x += FONT_WIDTH * term_scale;
    term_input_pos++;
    term_redraw_cursor(0);
}

void term_handle_type(uint8_t scancode) {
    if (scancode == KEY_ARROW_LEFT) return term_handle_left();
    else if (scancode == KEY_ARROW_RIGHT) return term_handle_right();

    char c = scancode_to_char(scancode);
    if (!c) return;

    if (c == '\b') term_handle_backspace();

    char s[2] = { c, '\0' };
    term_write(s, COLOR_WHITE, COLOR_BLACK);

    if (c != '\b' && c != '\n') {
        term_input_pos++;
        term_input[term_input_cursor++] = c;
        term_input[term_input_cursor] = '\0';
    }

    if (c == '\n') {
        command_handle(term_input);
        term_input_cursor = 0;
        term_input_pos = 0;
        term_input[0] = '\0';
    }
}
