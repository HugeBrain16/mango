#include <stddef.h>
#include "editor.h"
#include "keyboard.h"
#include "screen.h"
#include "font.h"
#include "color.h"
#include "pit.h"
#include "heap.h"
#include "string.h"
#include "terminal.h"
#include "file.h"

static uint32_t cursor_ticks = 0;
static int cursor_visible = 0;

void edit_init(uint32_t file) {
    edit_node = file;
    line_count = 1;
    edit_line = 0;
    edit_pos = 0;
    edit_cursor = 0;
    edit_x = 0;
    edit_y = 0;

    char *data = file_read(file);
    for (int i = 0; data[i] != '\0'; i++)
        if (data[i] == '\n') line_count++;

    edit_buffer = (char**) heap_alloc(line_count * sizeof(char*));
    for (int i = 0; i < line_count; i++) {
        edit_buffer[i] = (char*) heap_alloc(EDIT_MAX_SIZE * sizeof(char));
        memset(edit_buffer[i], 0, EDIT_MAX_SIZE);
    }

    int line = 0;
    int line_cursor = 0;
    for (size_t i = 0; i < strlen(data); i++) {
        if (data[i] != '\n') {
            edit_buffer[line][line_cursor++] = data[i];
            screen_draw_char(edit_x, edit_y, data[i], COLOR_WHITE, COLOR_BLACK, screen_scale);
            edit_x += FONT_WIDTH * screen_scale;
        } else {
            edit_buffer[line][line_cursor] = '\0';
            line++;
            line_cursor = 0;
            edit_x = 0;
            edit_y += FONT_HEIGHT * screen_scale;
        }
    }

    edit_buffer[line][line_cursor] = '\0';

    edit_cursor = strlen(edit_buffer[line]);
    edit_line = line;
    edit_pos = 0;
    edit_x = 0;

    heap_free(data);
}

static void edit_clear_cursor() {
    if (edit_pos < edit_cursor)
        screen_draw_char(edit_x, edit_y, edit_buffer[edit_line][edit_pos], COLOR_WHITE, COLOR_BLACK, screen_scale);
    else
        screen_draw_char(edit_x, edit_y, ' ', COLOR_WHITE, COLOR_BLACK, screen_scale);
}

void edit_draw_cursor() {
    if (pit_ticks - cursor_ticks >= 50) {
        cursor_visible = !cursor_visible;
        cursor_ticks = pit_ticks;

        edit_clear_cursor();

        if (cursor_visible)
            screen_draw_char(edit_x, edit_y, '_', COLOR_WHITE, COLOR_TRANSPARENT, screen_scale);
    }
}

static void edit_redraw_cursor(int hide) {
    cursor_visible = hide;
    cursor_ticks = 0;
    edit_draw_cursor();
}

void edit_write(const char *text, uint32_t fg_color, uint32_t bg_color) {
    edit_redraw_cursor(1);

    for (size_t i = 0; i < strlen(text); i++) {
        char c = text[i];

        if (c != '\n')
            screen_draw_char(edit_x, edit_y, c, fg_color, bg_color, screen_scale); 
        if (c != '\b')
            edit_x += FONT_WIDTH * screen_scale;
        if (c == '\n') {
            edit_x = 0;
            edit_y += FONT_HEIGHT * screen_scale;

            int max_y = screen_height - (FONT_HEIGHT * screen_scale);
            if (edit_y > max_y) {
                screen_scroll(FONT_HEIGHT * screen_scale);
                edit_y = max_y;
            }
        }
    }
}

static void edit_handle_backspace() {
    if (edit_pos == 0) return;

    edit_clear_cursor();

    for (int i = edit_pos - 1; i < edit_cursor - 1; i++) {
        edit_buffer[edit_line][i] = edit_buffer[edit_line][i + 1];
    }
    edit_buffer[edit_line][--edit_cursor] = '\0';
    edit_pos--;

    int saved_x = edit_x - FONT_WIDTH * screen_scale;
    edit_x = saved_x;

    for (int i = edit_pos; i < edit_cursor; i++) {
        screen_draw_char(edit_x, edit_y, edit_buffer[edit_line][i], COLOR_WHITE, COLOR_BLACK, screen_scale);
        edit_x += FONT_WIDTH * screen_scale;
    }

    screen_draw_char(edit_x, edit_y, ' ', COLOR_WHITE, COLOR_BLACK, screen_scale);
    edit_x = saved_x;

    edit_redraw_cursor(0);
}

static void edit_handle_left() {
    if (edit_pos == 0 || edit_x == 0) return;

    edit_clear_cursor();
    edit_x -= FONT_WIDTH * screen_scale;
    edit_pos--;
    edit_redraw_cursor(0);
}

static void edit_handle_right() {
    int max_x = screen_width - (FONT_WIDTH * screen_scale);
    if (edit_pos == edit_cursor || edit_x >= max_x) return;

    edit_clear_cursor();
    edit_x += FONT_WIDTH * screen_scale;
    edit_pos++;
    edit_redraw_cursor(0);
}

static void edit_handle_up() {
    if (edit_line == 0) return;

    edit_clear_cursor();
    edit_line--;
    edit_pos = 0;
    edit_cursor = strlen(edit_buffer[edit_line]);
    edit_x = 0;
    edit_y -= FONT_HEIGHT * screen_scale;
    edit_redraw_cursor(0);
}

static void edit_handle_down() {
    if (edit_line + 1 >= line_count) {
        line_count++;
        edit_buffer = (char**) heap_realloc(edit_buffer, line_count * sizeof(char*));
        edit_buffer[line_count - 1] = (char*) heap_alloc(EDIT_MAX_SIZE * sizeof(char));
        memset(edit_buffer[line_count - 1], 0, EDIT_MAX_SIZE);
    }

    edit_clear_cursor();
    edit_line++;
    edit_pos = 0;
    edit_cursor = strlen(edit_buffer[edit_line]);
    edit_x = 0;
    edit_y += FONT_HEIGHT * screen_scale;
    edit_redraw_cursor(0);
}

static void edit_handle_save() {
    int max_line = -1;
    for (int i = 0; i < line_count; i++) {
        if (edit_buffer[i][0] != '\0') {
            max_line = i;
        }
    }

    size_t buffer_size = 128 * (max_line + 1);
    char *buffer = heap_alloc(buffer_size);
    memset(buffer, 0, buffer_size);

    for (int i = 0; i < max_line + 1; i++) {
        strcat(buffer, edit_buffer[i]);

        if (i < max_line)
            strcat(buffer, "\n");
    }

    file_write(edit_node, buffer, buffer_size);
    heap_free(buffer);
    for (int i = 0; i < line_count; i++)
        heap_free(edit_buffer[i]);
    heap_free(edit_buffer);
    edit_line = 0;
    edit_pos = 0;
    edit_cursor = 0;
    edit_x = 0;
    edit_y = 0;

    keyboard_mode = KEYBOARD_MODE_TERM;
    screen_clear(COLOR_BLACK);
    term_x = 0;
    term_y = 0;
    term_write("\n> ", COLOR_WHITE, COLOR_BLACK);
    term_prompt = term_x;
}

void edit_handle_type(uint8_t scancode) {
    if (scancode == KEY_ESC) return edit_handle_save();

    if (scancode == KEY_ARROW_LEFT) return edit_handle_left();
    else if (scancode == KEY_ARROW_RIGHT) return edit_handle_right();
    else if (scancode == KEY_ARROW_UP) return edit_handle_up();
    else if (scancode == KEY_ARROW_DOWN) return edit_handle_down();

    char c = scancode_to_char(scancode);
    if (!c) return;

    if (c == '\b') return edit_handle_backspace();

    int max_x = screen_width - (FONT_WIDTH * screen_scale);
    if (edit_x >= max_x) return;

    if (c != '\n') {
        if (edit_cursor >= EDIT_MAX_SIZE - 1) return;

        edit_clear_cursor();

        for (int i = edit_cursor; i > edit_pos; i--)
            edit_buffer[edit_line][i] = edit_buffer[edit_line][i - 1];

        edit_buffer[edit_line][edit_pos] = c;
        edit_buffer[edit_line][++edit_cursor] = '\0';

        int saved_x = edit_x;
        for (int i = edit_pos; i < edit_cursor; i++) {
            screen_draw_char(edit_x, edit_y, edit_buffer[edit_line][i], COLOR_WHITE, COLOR_BLACK, screen_scale);
            edit_x += FONT_WIDTH * screen_scale;
        }

        edit_pos++;
        edit_x = saved_x + FONT_WIDTH * screen_scale;
        edit_redraw_cursor(0);
    } else {
        edit_handle_down();
    }
}
