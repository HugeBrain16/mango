#include "terminal.h"
#include "string.h"
#include "screen.h"
#include "font.h"
#include "pit.h"
#include "color.h"
#include "keyboard.h"
#include "command.h"
#include "pic.h"
#include "list.h"
#include "heap.h"
#include "serial.h"

static uint32_t cursor_ticks = 0;
static int cursor_visible = 0;
static list_t *term_history = NULL;
static size_t term_history_idx = 0;

void term_init() {
    term_x = 0;
    term_y = 0;
    term_input_cursor = 0;
    term_input_pos = 0;
    term_input_buffer = NULL;

    if (!term_history) {
        term_history = heap_alloc(sizeof(list_t));
        list_init(term_history);
        term_history_idx = 0;
    }
}

static void term_clear_cursor() {
    if (term_input_pos < term_input_cursor)
        screen_draw_char(term_x, term_y, term_input[term_input_pos], COLOR_WHITE, COLOR_BLACK, screen_scale);
    else
        screen_draw_char(term_x, term_y, ' ', COLOR_WHITE, COLOR_BLACK, screen_scale);
}

void term_draw_cursor() {
    uint32_t cursor_delay = (TERM_CURSOR_BLINK * pit_hz) / 1000;

    if (pit_ticks - cursor_ticks >= cursor_delay) {
        cursor_visible = !cursor_visible;
        cursor_ticks = pit_ticks;

        term_clear_cursor();

        if (cursor_visible)
            screen_draw_char(term_x, term_y, '_', COLOR_WHITE, COLOR_TRANSPARENT, screen_scale);

        screen_flush();
    }
}

static void term_redraw_cursor(int hide) {
    cursor_visible = hide;
    cursor_ticks = 0;
    term_draw_cursor();
}

void term_write(const char *text, uint32_t fg_color, uint32_t bg_color) {
    term_clear_cursor();

    for (const char *p = text; *p != '\0'; p++) {
        char c = *p;

        if (c != '\n')
            screen_draw_char(term_x, term_y, c, fg_color, bg_color, screen_scale); 
        if (c != '\b')
            term_x += FONT_WIDTH * screen_scale;
        if (c == '\n') {
            term_x = 0;
            term_y += FONT_HEIGHT * screen_scale;

            int max_y = screen_height - (FONT_HEIGHT * screen_scale);
            if (term_y > max_y) {
                screen_scroll(FONT_HEIGHT * screen_scale);
                term_y = max_y;
            }
        }
    }
}

static void term_handle_backspace() {
    if (term_input_pos == 0) return;

    term_clear_cursor();

    for (int i = term_input_pos - 1; i < term_input_cursor - 1; i++) {
        term_input[i] = term_input[i + 1];
    }
    term_input[--term_input_cursor] = '\0';
    term_input_pos--;

    int saved_x = term_x - FONT_WIDTH * screen_scale;
    term_x = saved_x;

    for (int i = term_input_pos; i < term_input_cursor; i++) {
        screen_draw_char(term_x, term_y, term_input[i], COLOR_WHITE, COLOR_BLACK, screen_scale);
        term_x += FONT_WIDTH * screen_scale;
    }

    screen_draw_char(term_x, term_y, ' ', COLOR_WHITE, COLOR_BLACK, screen_scale);
    term_x = saved_x;

    term_redraw_cursor(0);

    screen_flush();
}

static void term_handle_left() {
    if (term_input_pos == 0) return;

    term_clear_cursor();
    term_x -= FONT_WIDTH * screen_scale;
    term_input_pos--;
    term_redraw_cursor(0);

    screen_flush();
}

static void term_handle_right() {
    if (term_input_pos == term_input_cursor) return;

    term_clear_cursor();
    term_x += FONT_WIDTH * screen_scale;
    term_input_pos++;
    term_redraw_cursor(0);

    screen_flush();
}

void term_get_input(const char* prompt, char *buffer, size_t size, uint32_t fg_color, uint32_t bg_color) {
    memset(buffer, 0, size);
    term_input_buffer = buffer;
    term_write(prompt, fg_color, bg_color);
    term_prompt = term_x;

    __asm__ volatile("sti");
    while (term_input_buffer != NULL) {
        __asm__ volatile("hlt");
    }
}

static void term_handle_history(int direction) {
    if (direction == -1 && term_history_idx <= 1) {
        int draw_x = term_prompt;
        for (size_t i = 0; i < strlen(term_input) + 1; i++) {
            screen_draw_char(draw_x, term_y, ' ', COLOR_WHITE, COLOR_BLACK, screen_scale);
            draw_x += FONT_WIDTH * screen_scale;
        }
        term_x = term_prompt;
        term_input[0] = '\0';
        term_input_cursor = 0;
        term_input_pos = 0;

        if (term_history_idx > 0)
            term_history_idx += direction;
        screen_flush();
        return;
    }

    if (direction == 1 && term_history_idx == term_history->size) return;
    term_history_idx += direction;

    char *line = list_get(term_history, term_history->size - term_history_idx);
    if (!line)
        return;

    int draw_x = term_prompt;
    for (size_t i = 0; i < strlen(term_input) + 1; i++) {
        screen_draw_char(draw_x, term_y, ' ', COLOR_WHITE, COLOR_BLACK, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
    }
    term_x = term_prompt;

    size_t length = strlen(line) + 1;
    strncpy(term_input, line, length);
    term_input_cursor = length - 1;
    term_input_pos = term_input_cursor;

    for (int i = 0; i < term_input_cursor; i++) {
        screen_draw_char(term_x, term_y, term_input[i], COLOR_WHITE, COLOR_BLACK, screen_scale);
        term_x += FONT_WIDTH * screen_scale;
    }

    term_redraw_cursor(0);

    screen_flush();
}

void term_handle_type(uint8_t scancode) {
    if (scancode == KEY_ARROW_LEFT) return term_handle_left();
    else if (scancode == KEY_ARROW_RIGHT) return term_handle_right();
    else if (scancode == KEY_ARROW_UP) return term_handle_history(1);
    else if (scancode == KEY_ARROW_DOWN) return term_handle_history(-1);

    char c = scancode_to_char(scancode);
    if (!c || c == '\t') return;

    if (c == '\b') return term_handle_backspace();

    if (c != '\n') {
        term_clear_cursor();

        for (int i = term_input_cursor; i > term_input_pos; i--)
            term_input[i] = term_input[i - 1];

        term_input[term_input_pos] = c;
        term_input[++term_input_cursor] = '\0';

        int saved_x = term_x;
        for (int i = term_input_pos; i < term_input_cursor; i++) {
            screen_draw_char(term_x, term_y, term_input[i], COLOR_WHITE, COLOR_BLACK, screen_scale);
            term_x += FONT_WIDTH * screen_scale;
        }

        term_input_pos++;
        term_x = saved_x + FONT_WIDTH * screen_scale;
        term_redraw_cursor(0);
    } else {
        char s[2] = { c, '\0' };
        term_write(s, COLOR_WHITE, COLOR_BLACK);
        term_history_idx = 0;
        term_input_cursor = 0;
        term_input_pos = 0;

        if (term_input_buffer != NULL) {
            strcpy(term_input_buffer, term_input);
            term_input_buffer = NULL;
        } else {
            strtrim(term_input);
            if (term_input[0] != '\0') {
                size_t length = strlen(term_input) + 1;
                char *line = heap_alloc(length);
                strncpy(line, term_input, length);
                list_push(term_history, line);
            }
            command_handle(term_input, 1);
        }

        term_input[0] = '\0';
    }

    screen_flush();
}