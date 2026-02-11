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
static size_t edit_column = 0;
static size_t edit_showfrom = 1; // by line

static size_t get_block_size() {
    file_node_t file;
    file_node(edit_node, &file);

    file_data_t block;
    file_data(file.first_block, &block);

    return sizeof(block.data);
}

static size_t find_line_start(size_t pos) {
    size_t p = pos;
    while (p > 0 && edit_buffer[p - 1] != '\n')
        p--;
    return p;
}

static size_t find_line_end(size_t pos) {
    size_t p = pos;
    while (p < edit_cursor && edit_buffer[p] != '\n')
        p++;
    return p;
}

static size_t get_line_length(size_t pos) {
    size_t start = find_line_start(pos);
    return pos - start;
}

static size_t get_x_from_pos(size_t pos) {
    size_t line_len = get_line_length(pos);
    return line_len * FONT_WIDTH * screen_scale;
}

static void clear(size_t x, size_t y) {
    size_t draw_x = x;
    while (draw_x < screen_width) {
        screen_draw_char(draw_x, y, ' ', COLOR_WHITE, COLOR_BLACK, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
    }
}

static void redraw_from_pos(size_t start_pos, size_t x, size_t y) {
    size_t draw_x = x;
    size_t draw_y = y;
    size_t i = start_pos;

    while (i < edit_cursor) {
        if (edit_buffer[i] != '\n') {
            screen_draw_char(draw_x, draw_y, edit_buffer[i], COLOR_WHITE, COLOR_BLACK, screen_scale);
            draw_x += FONT_WIDTH * screen_scale;
        } else {
            clear(draw_x, draw_y);
            draw_x = 0;
            draw_y += FONT_HEIGHT * screen_scale;
        }
        i++;
    }

    clear(draw_x, draw_y);

    draw_y += FONT_HEIGHT * screen_scale;
    clear(0, draw_y);
}

static void redraw_line(size_t start_pos, size_t x, size_t y) {
    size_t draw_x = x;
    size_t i = start_pos;

    while (i < edit_cursor && edit_buffer[i] != '\n') {
        screen_draw_char(draw_x, y, edit_buffer[i], COLOR_WHITE, COLOR_BLACK, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
        i++;
    }

    screen_draw_char(draw_x, y, ' ', COLOR_WHITE, COLOR_BLACK, screen_scale);
}

static size_t get_pos_at_column(size_t line_start, size_t column) {
    size_t line_end = find_line_end(line_start);
    size_t line_len = line_end - line_start;

    if (column >= line_len)
        return line_start + line_len;

    return line_start + column;
}

void edit_init(uint32_t file_sector) {
    edit_node = file_sector;

    edit_buffer = file_read(file_sector);
    edit_pos = 0;

    size_t draw_x = 0;
    size_t draw_y = 0;

    char *c = edit_buffer;
    while (*c != '\0') {
        if (*c != '\n') {
            screen_draw_char(draw_x, draw_y, *c, COLOR_WHITE, COLOR_BLACK, screen_scale);
            draw_x += FONT_WIDTH * screen_scale;
        } else {
            draw_x = 0;
            draw_y += FONT_HEIGHT * screen_scale;
        }

        c++;
        edit_cursor++;
    }
}

static void edit_clear_cursor() {
    if (edit_pos < edit_cursor) {
        char c = edit_buffer[edit_pos];

        if (c == '\n')
            screen_draw_char(edit_x, edit_y, ' ', COLOR_WHITE, COLOR_BLACK, screen_scale);
        else
            screen_draw_char(edit_x, edit_y, edit_buffer[edit_pos], COLOR_WHITE, COLOR_BLACK, screen_scale);
    } else
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

static void edit_handle_scroll() {
    edit_clear_cursor();

    size_t top_y = FONT_HEIGHT * screen_scale;
    size_t bottom_y = screen_height - top_y;
    size_t old_showfrom = edit_showfrom;

    if (edit_y > bottom_y)
        edit_showfrom++;
    else if (edit_y < top_y && edit_showfrom > 1)
        edit_showfrom--;

    if (old_showfrom != edit_showfrom) {
        size_t line = 1;

        size_t draw_x = 0;
        size_t draw_y = 0;

        screen_clear(COLOR_BLACK);

        for (size_t i = 0; i < edit_cursor; i++) {
            if (line >= edit_showfrom) {
                if (edit_buffer[i] == '\n') {
                    draw_y += FONT_HEIGHT * screen_scale;
                    draw_x = 0;
                } else {
                    screen_draw_char(draw_x, draw_y,
                        edit_buffer[i],
                        COLOR_WHITE, COLOR_BLACK,
                        screen_scale);

                    draw_x += FONT_WIDTH * screen_scale;
                }
            }

            if (edit_buffer[i] == '\n')
                line++;
        }

        if (line >= edit_showfrom)
            clear(draw_x, draw_y);

        if (old_showfrom < edit_showfrom)
            edit_y = bottom_y;
        else if (old_showfrom > edit_showfrom)
            edit_y = top_y;
    }

    edit_redraw_cursor(0);
}

static void edit_handle_backspace() {
    if (edit_pos == 0) return;

    edit_clear_cursor();

    char deleted = edit_buffer[edit_pos - 1];

    for (size_t i = edit_pos - 1; i < edit_cursor; i++)
        edit_buffer[i] = edit_buffer[i + 1];

    edit_cursor--;
    edit_pos--;

    if (deleted == '\n') {
        edit_y -= FONT_HEIGHT * screen_scale;
        edit_x = get_x_from_pos(edit_pos);
        redraw_from_pos(edit_pos, edit_x, edit_y);
    } else {
        edit_x -= FONT_WIDTH * screen_scale;
        redraw_line(edit_pos, edit_x, edit_y);
    }

    edit_column = get_line_length(edit_pos);
    edit_handle_scroll();
}

static void edit_handle_left() {
    if (edit_pos == 0) return;

    edit_clear_cursor();
    edit_pos--;

    if (edit_buffer[edit_pos] == '\n') {
        edit_y -= FONT_HEIGHT * screen_scale;
        edit_x = get_x_from_pos(edit_pos);
    } else {
        edit_x -= FONT_WIDTH * screen_scale;
    }

    edit_column = get_line_length(edit_pos);
    edit_handle_scroll();
}

static void edit_handle_right() {
    if (edit_pos == edit_cursor) return;

    edit_clear_cursor();

    if (edit_buffer[edit_pos] == '\n') {
        edit_pos++;
        edit_y += FONT_HEIGHT * screen_scale;
        edit_x = 0;
    } else {
        edit_pos++;
        edit_x += FONT_WIDTH * screen_scale;
    }

    edit_column = get_line_length(edit_pos);
    edit_handle_scroll();
}

static void edit_handle_up() {
    if (edit_pos == 0) return;

    edit_clear_cursor();

    size_t line_start = find_line_start(edit_pos);
    if (line_start == 0) {
        edit_redraw_cursor(0);
        return;
    }

    size_t prev_line_end = line_start - 1;
    size_t prev_line_start = find_line_start(prev_line_end);

    edit_pos = get_pos_at_column(prev_line_start, edit_column);
    edit_x = get_x_from_pos(edit_pos);
    edit_y -= FONT_HEIGHT * screen_scale;

    edit_handle_scroll();
}

static void edit_handle_down() {
    if (edit_pos >= edit_cursor) return;

    edit_clear_cursor();

    size_t line_end = find_line_end(edit_pos);
    if (line_end >= edit_cursor) {
        edit_redraw_cursor(0);
        return;
    }

    size_t next_line_start = line_end + 1;

    edit_pos = get_pos_at_column(next_line_start, edit_column);
    edit_x = get_x_from_pos(edit_pos);
    edit_y += FONT_HEIGHT * screen_scale;

    edit_handle_scroll();
}

static void edit_handle_save() {
    file_write(edit_node, edit_buffer, edit_cursor + 1);
    heap_free(edit_buffer);
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

    size_t max_x = screen_width - (FONT_WIDTH * screen_scale);
    if (edit_x >= max_x && c != '\n') return;

    edit_clear_cursor();

    if (edit_buffer[edit_cursor] != '\0') {
        size_t old_size = edit_cursor + 1;
        size_t new_size = old_size + get_block_size();
        edit_buffer = heap_realloc(edit_buffer, new_size);

        for (size_t i = old_size; i < new_size; i++)
            edit_buffer[i] = '\0';
    }

    for (size_t i = edit_cursor; i > edit_pos; i--)
        edit_buffer[i] = edit_buffer[i - 1];

    edit_buffer[edit_pos] = c;
    edit_cursor++;
    edit_pos++;

    if (c == '\n') {
        redraw_from_pos(edit_pos - 1, edit_x, edit_y);
        edit_x = 0;
        edit_y += FONT_HEIGHT * screen_scale;

        edit_handle_scroll();
    } else {
        screen_draw_char(edit_x, edit_y, c, COLOR_WHITE, COLOR_BLACK, screen_scale);
        edit_x += FONT_WIDTH * screen_scale;
        redraw_line(edit_pos, edit_x, edit_y);
    }

    edit_column = get_line_length(edit_pos);
    edit_redraw_cursor(0);
}
