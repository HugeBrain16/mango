#include "terminal.h"
#include "string.h"
#include "screen.h"
#include "font.h"
#include "pit.h"
#include "keyboard.h"
#include "command.h"
#include "pic.h"
#include "list.h"
#include "heap.h"
#include "serial.h"
#include "config.h"
#include "file.h"

static uint32_t cursor_ticks = 0;
static int cursor_visible = 0;
static list_t *term_history = NULL;
static size_t term_history_idx = 0;
static int term_history_max = TERM_MAX_HISTORY;
static char *term_path = NULL;

void term_init() {
    term_x = 0;
    term_y = 0;
    term_input_cursor = 0;
    term_input_pos = 0;
    term_input_buffer = NULL;
    term_fg = COLOR_WHITE;
    term_bg = COLOR_BLACK;

    if (!term_history) {
        term_history = heap_alloc(sizeof(list_t));
        list_init(term_history);
        term_history_idx = 0;
    }

    if (!term_path)
        term_path = heap_alloc(FILE_MAX_PATH);
}

void term_load_config() {
    char *value;
    int col;

    value = config_get("/system/config/terminal.cfg", "color_fg");
    if (value) {
        col = color(value);
        if (col != COLOR_INVALID)
            term_fg = col;
        else
            term_fg = TERM_COLOR_FG;
        heap_free(value);
    }

    value = config_get("/system/config/terminal.cfg", "color_bg");
    if (value) {
        int col = color(value);
        if (col != COLOR_INVALID)
            term_bg = col;
        else
            term_bg = TERM_COLOR_BG;
        heap_free(value);
    }

    value = config_get("/system/config/shell.cfg", "max_history");
    if (value) {
        int max = intstr(value);
        if (max > 0)
            term_history_max = max;
        else
            term_history_max = TERM_MAX_HISTORY;
        heap_free(value);
    }
}

static void term_clear_cursor() {
    if (term_input_pos < term_input_cursor) {
        char c = term_input[term_input_pos];
        if (c == '\t')
            c = ' ';

        screen_draw_char(term_x, term_y, c, term_fg, term_bg, screen_scale);
    } else
        screen_draw_char(term_x, term_y, ' ', term_fg, term_bg, screen_scale);
}

void term_draw_cursor() {
    uint32_t cursor_delay = (TERM_CURSOR_BLINK * pit_hz) / 1000;

    if (pit_ticks - cursor_ticks >= cursor_delay) {
        cursor_visible = !cursor_visible;
        cursor_ticks = pit_ticks;

        term_clear_cursor();

        if (cursor_visible)
            screen_draw_char(term_x, term_y, '_', term_fg, COLOR_TRANSPARENT, screen_scale);

        screen_flush();
    }
}

static void term_redraw_cursor(int hide) {
    cursor_visible = hide;
    cursor_ticks = 0;
    term_draw_cursor();
}

void term_write(const char *text) {
    term_clear_cursor();

    for (const char *p = text; *p != '\0'; p++) {
        char c = *p;

        if (c == '\t') {
            for (int i = 0; i < KEYBOARD_TAB_LENGTH; i++) {
                screen_draw_char(term_x, term_y, ' ', term_fg, term_bg, screen_scale);
                term_x += FONT_WIDTH * screen_scale;
            }
        } else if (c != '\n')
            screen_draw_char(term_x, term_y, c, term_fg, term_bg, screen_scale); 

        if (c != '\b' && c != '\t')
            term_x += FONT_WIDTH * screen_scale;
        if (c == '\n') {
            term_x = 0;
            term_y += FONT_HEIGHT * screen_scale;

            int max_y = screen_height - (FONT_HEIGHT * screen_scale);
            if (term_y > max_y) {
                screen_scroll(FONT_HEIGHT * screen_scale, term_bg);
                term_y = max_y;
            }
        }
    }
}

void term_write2(const char *msg, uint32_t fg_color, uint32_t bg_color) {
    int fg = term_fg;
    int bg = term_bg;
    term_fg = fg_color;
    term_bg = bg_color;

    term_write(msg);

    term_fg = fg;
    term_bg = bg;
}

static void term_handle_backspace() {
    if (term_input_pos == 0) return;

    term_clear_cursor();

    char deleted = term_input[term_input_pos - 1];

    for (int i = term_input_pos - 1; i < term_input_cursor - 1; i++)
        term_input[i] = term_input[i + 1];

    term_input[--term_input_cursor] = '\0';
    term_input_pos--;

    int draw_x = term_prompt;
    for (int i = 0; i < term_input_cursor; i++) {
        char c = term_input[i];

        if (c == '\t') {
            for (int j = 0; j < KEYBOARD_TAB_LENGTH; j++) {
                screen_draw_char(draw_x, term_y, ' ', term_fg, term_bg, screen_scale);
                draw_x += FONT_WIDTH * screen_scale;
            }
        } else {
            screen_draw_char(draw_x, term_y, c, term_fg, term_bg, screen_scale);
            draw_x += FONT_WIDTH * screen_scale;
        }
    }

    int max_x = screen_width - (FONT_WIDTH * screen_scale);
    while (draw_x < max_x) {
        screen_draw_char(draw_x, term_y, ' ', term_fg, term_bg, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
    }

    if (deleted == '\t')
        term_x -= (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;
    else
        term_x -= FONT_WIDTH * screen_scale;
    term_redraw_cursor(0);

    screen_flush();
}

static void term_handle_left() {
    if (term_input_pos == 0) return;

    term_clear_cursor();
    term_input_pos--;
    char c = term_input[term_input_pos];
    if (c == '\t') {
        term_x -= (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;
    } else if (term_x > term_prompt) {
        term_x -= FONT_WIDTH * screen_scale;
    } else {
        term_x = term_prompt;
    }

    term_redraw_cursor(0);

    screen_flush();
}

static void term_handle_right() {
    if (term_input_pos == term_input_cursor) return;

    term_clear_cursor();
    char b = term_input[term_input_pos];
    term_input_pos++;
    char c = term_input[term_input_pos];

    if (c == '\t' || b == '\t') {
        term_x += (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;

        if (b != '\t' && term_input_pos < term_input_cursor)
            term_input_pos++;
    }

    if (b != '\t')
        term_x += FONT_WIDTH * screen_scale;

    term_redraw_cursor(0);

    screen_flush();
}

void term_get_input(const char* prompt, char *buffer, size_t size) {
    memset(buffer, 0, size);
    term_input_buffer = buffer;
    term_write(prompt);
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
            screen_draw_char(draw_x, term_y, ' ', term_fg, term_bg, screen_scale);
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
        screen_draw_char(draw_x, term_y, ' ', term_fg, term_bg, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
    }
    term_x = term_prompt;

    size_t length = strlen(line) + 1;
    strncpy(term_input, line, length);
    term_input_cursor = length - 1;
    term_input_pos = term_input_cursor;

    for (int i = 0; i < term_input_cursor; i++) {
        screen_draw_char(term_x, term_y, term_input[i], term_fg, term_bg, screen_scale);
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
    if (!c) return;

    if (c == '\b') return term_handle_backspace();

    if (c != '\n') {
        term_clear_cursor();

        for (int i = term_input_cursor; i > term_input_pos; i--)
            term_input[i] = term_input[i - 1];

        term_input[term_input_pos] = c;
        term_input[++term_input_cursor] = '\0';

        int draw_x = term_prompt;
        for (int i = 0; i < term_input_cursor; i++) {
            char d = term_input[i];

            if (d == '\t') {
                for (int j = 0; j < KEYBOARD_TAB_LENGTH; j++) {
                    screen_draw_char(draw_x, term_y, ' ', term_fg, term_bg, screen_scale);
                    draw_x += FONT_WIDTH * screen_scale;
                }
            } else {
                screen_draw_char(draw_x, term_y, d, term_fg, term_bg, screen_scale);
                draw_x += FONT_WIDTH * screen_scale;
            }
        }

        if (c == '\t')
            term_x += (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;
        else
            term_x += FONT_WIDTH * screen_scale;

        term_input_pos++;
        term_redraw_cursor(0);
    } else {
        char s[2] = { c, '\0' };
        term_write(s);
        term_history_idx = 0;
        term_input_cursor = 0;
        term_input_pos = 0;

        if (term_input_buffer != NULL) {
            strcpy(term_input_buffer, term_input);
            term_input_buffer = NULL;
        } else {
            strtrim(term_input);
            if (term_input[0] != '\0') {
                while ((int)term_history->size >= term_history_max) {
                    heap_free(list_pop(term_history));
                }

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

void term_update_path() {
    if (config_has("/system/config/shell.cfg", "show_path"))
        file_get_abspath(file_current, term_path, FILE_MAX_PATH);
}

void term_draw_prompt() {
    char *symbol = ">";

    char *prompt = config_get("/system/config/shell.cfg", "prompt");
    if (prompt) {
        if (!strcmp(prompt, "arrow")) symbol = ">";
        else if (!strcmp(prompt, "tag") || !strcmp(prompt, "hashtag")) symbol = "#";
        else if (!strcmp(prompt, "dollar")) symbol = "$";
        else if (!strcmp(prompt, "tilde")) symbol = "~";
    }

    term_write("\n");

    if (config_has("/system/config/shell.cfg", "show_path")) {
        term_write(term_path);
        term_write(" ");
    }

    term_write(symbol);
    term_write(" ");
    term_prompt = term_x;

    heap_free(prompt);
}
