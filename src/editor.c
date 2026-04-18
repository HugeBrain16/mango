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
static int edit_column = 0;
static int edit_showfrom = 1; // by line

static size_t get_block_size() {
    file_node_t file;
    file_node(edit_node, &file);

    file_data_t block;
    file_data(file.first_block, &block);

    return sizeof(block.data);
}

static int find_line_start(int pos) {
    while (pos > 0 && edit_buffer[pos - 1] != '\n')
        pos--;

    return pos;
}

static size_t find_line_end(size_t pos) {
    while (pos < edit_cursor && edit_buffer[pos] != '\n')
        pos++;

    return pos;
}

static int get_line_count(size_t end) {
    int line = 1;

    for (size_t i = 0; i < end; i++) {
        if (edit_buffer[i] == '\n')
            line++;
    }

    return line;
}

static int get_line_length(int pos) {
    int start = find_line_start(pos);
    return pos - start;
}

static int get_x_from_pos(int pos) {
    int line_len = get_line_length(pos);
    return line_len * FONT_WIDTH * screen_scale;
}

static void clear(int x, int y, uint32_t color) {
    int draw_x = x;
    while (draw_x < screen_width) {
        screen_draw_char(draw_x, y, ' ', color, color, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
    }
}

static void redraw_line(int start_pos, int x, int y) {
    size_t i = start_pos;

    while (i < edit_cursor && edit_buffer[i] != '\n') {
        char c = edit_buffer[i];

        if (c == '\t') {
            for (int j = 0; j < KEYBOARD_TAB_LENGTH; j++) {
                screen_draw_char(x, y, ' ', EDITOR_FG, EDITOR_BG, screen_scale);
                x += FONT_WIDTH * screen_scale;
            }
        } else {
            screen_draw_char(x, y, c, EDITOR_FG, EDITOR_BG, screen_scale);
            x += FONT_WIDTH * screen_scale;
        }

        i++;
    }
}

static void redraw_from_pos(int start_pos, int x, int y) {
    size_t i = start_pos;

    while (i < edit_cursor) {
        char c = edit_buffer[i];

        if (c != '\n') {
            if (c == '\t')
                for (int j = 0; j < KEYBOARD_TAB_LENGTH; j++) {
                    screen_draw_char(x, y, ' ', EDITOR_FG, EDITOR_BG, screen_scale);
                    x += FONT_WIDTH * screen_scale;
                }
            else {
                screen_draw_char(x, y, c, EDITOR_FG, EDITOR_BG, screen_scale);
                x += FONT_WIDTH * screen_scale;
            }
        } else {
            clear(x, y, EDITOR_BG);
            x = 0;
            y += FONT_HEIGHT * screen_scale;
        }

        i++;
    }

    clear(x, y, EDITOR_BG);

    y += FONT_HEIGHT * screen_scale;
    clear(0, y, EDITOR_BG);
}

static int get_pos_at_column(int line_start, int column) {
    int line_end = find_line_end(line_start);
    int line_len = line_end - line_start;

    if (column >= line_len)
        return line_start + line_len;

    return line_start + column;
}

static void edit_statusbar_draw();

void edit_init(uint32_t file_sector) {
    edit_node = file_sector;

    edit_buffer = file_read(file_sector);
    edit_pos = 0;

    int draw_x = 0;
    int draw_y = 0;

    screen_clear(EDITOR_BG);

    char *c = edit_buffer;
    while (*c != '\0') {
        if (*c != '\n') {
            if (*c == '\t') {
                for (int i = 0; i < KEYBOARD_TAB_LENGTH; i++) {
                    screen_draw_char(draw_x, draw_y, ' ', EDITOR_FG, EDITOR_BG, screen_scale);
                    draw_x += FONT_WIDTH * screen_scale;
                }
            } else {
                screen_draw_char(draw_x, draw_y, *c, EDITOR_FG, EDITOR_BG, screen_scale);
                draw_x += FONT_WIDTH * screen_scale;
            }
        } else {
            draw_x = 0;
            draw_y += FONT_HEIGHT * screen_scale;
        }

        c++;
        edit_cursor++;
    }

    edit_statusbar_draw();
}

static void edit_clear_cursor() {
    if (edit_pos < edit_cursor) {
        char c = edit_buffer[edit_pos];

        if (c == '\n' || c == '\t')
            screen_draw_char(edit_x, edit_y, ' ', EDITOR_FG, EDITOR_BG, screen_scale);
        else
            screen_draw_char(edit_x, edit_y, c, EDITOR_FG, EDITOR_BG, screen_scale);
    } else
        screen_draw_char(edit_x, edit_y, ' ', EDITOR_FG, EDITOR_BG, screen_scale);
}

void edit_draw_cursor() {
    uint32_t cursor_delay = (EDITOR_CURSOR_BLINK * pit_hz) / 1000;

    if (pit_ticks - cursor_ticks >= cursor_delay) {
        cursor_visible = !cursor_visible;
        cursor_ticks = pit_ticks;

        edit_clear_cursor();

        if (cursor_visible)
            screen_draw_char(edit_x, edit_y, '_', EDITOR_FG, COLOR_TRANSPARENT, screen_scale);

        screen_flush();
    }
}

static void edit_redraw_cursor(int hide) {
    cursor_visible = hide;
    cursor_ticks = 0;
    edit_draw_cursor();
}

static void edit_statusbar_draw() {
    file_node_t file;
    file_node(edit_node, &file);

    int draw_x = 0;
    int draw_y = screen_height - ((FONT_WIDTH * screen_scale) * 2);

    clear(draw_x, draw_y, EDITOR_STATUS_BG);

    char status[128];
    strfmt(status, "%s | Line %d-%d | Chars %d | Pos %d | Save: Ctrl+s, Quit: Esc",
        file.name,
        get_line_count(edit_pos),
        get_line_count(edit_cursor),
        edit_cursor,
        edit_pos);

    draw_x = 0;
    for (const char *p = status; *p != '\0'; p++) {
        screen_draw_char(draw_x, draw_y, *p, EDITOR_STATUS_FG, COLOR_TRANSPARENT, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
    }

    screen_flush();
}

static void edit_handle_scroll() {
    edit_clear_cursor();

    int top_y = FONT_HEIGHT * screen_scale;
    int bottom_y = screen_height - (top_y * 2);
    int old_showfrom = edit_showfrom;

    if (edit_y > bottom_y)
        edit_showfrom++;
    else if (edit_y < top_y && edit_showfrom > 1)
        edit_showfrom--;

    if (old_showfrom != edit_showfrom) {
        int line = 1;

        int draw_x = 0;
        int draw_y = 0;

        screen_clear(EDITOR_BG);

        for (size_t i = 0; i < edit_cursor; i++) {
            if (line >= edit_showfrom) {
                if (edit_buffer[i] == '\n') {
                    draw_y += FONT_HEIGHT * screen_scale;
                    draw_x = 0;
                } else {
                    if (edit_buffer[i] == '\t') {
                        for (int j = 0; j < KEYBOARD_TAB_LENGTH; j++) {
                            screen_draw_char(draw_x, draw_y, ' ', EDITOR_FG, EDITOR_BG, screen_scale);
                            draw_x += FONT_WIDTH * screen_scale;
                        }
                    } else {
                        screen_draw_char(draw_x, draw_y, edit_buffer[i], EDITOR_FG, EDITOR_BG, screen_scale);
                        draw_x += FONT_WIDTH * screen_scale;
                    }
                }
            }

            if (edit_buffer[i] == '\n')
                line++;
        }

        if (line >= edit_showfrom)
            clear(draw_x, draw_y, EDITOR_BG);

        if (old_showfrom < edit_showfrom)
            edit_y = bottom_y;
        else if (old_showfrom > edit_showfrom)
            edit_y = top_y;
    }

    edit_redraw_cursor(0);
    edit_statusbar_draw();
}

static void edit_handle_backspace() {
    if (edit_pos == 0) return;

    edit_clear_cursor();

    char deleted = edit_buffer[edit_pos - 1];

    for (size_t i = edit_pos - 1; i < edit_cursor; i++)
        edit_buffer[i] = edit_buffer[i + 1];

    edit_buffer[--edit_cursor] = '\0';
    edit_pos--;

    if (deleted == '\n') {
        edit_y -= FONT_HEIGHT * screen_scale;
        edit_x = get_x_from_pos(edit_pos);
        redraw_from_pos(edit_pos, edit_x, edit_y);
    } else {
        clear(0, edit_y, EDITOR_BG);
        redraw_line(find_line_start(edit_pos), 0, edit_y);

        if (deleted == '\t')
            edit_x -= (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;
        else
            edit_x -= FONT_WIDTH * screen_scale;
    }

    edit_column = get_line_length(edit_pos);
    edit_handle_scroll();
}

static void edit_handle_left() {
    if (edit_pos == 0) return;

    edit_clear_cursor();
    edit_pos--;

    char c = edit_buffer[edit_pos];
    if (c == '\n') {
        edit_y -= FONT_HEIGHT * screen_scale;
        edit_x = get_x_from_pos(edit_pos);
    } else {
        if (c == '\t')
            edit_x -= (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;
        else if (edit_x > 0)
            edit_x -= FONT_WIDTH * screen_scale;
        else
            edit_x = 0;
    }

    edit_column = get_line_length(edit_pos);
    edit_handle_scroll();
}

static void edit_handle_right() {
    if (edit_pos == edit_cursor) return;

    edit_clear_cursor();

    /* increment inside branch so shifting between lines isnt weird */
    if (edit_buffer[edit_pos] == '\n') {
        edit_pos++;
        edit_y += FONT_HEIGHT * screen_scale;
        edit_x = 0;
    } else {
        char b = edit_buffer[edit_pos];
        edit_pos++;
        char c = edit_buffer[edit_pos];

        if (c == '\t' || b == '\t') {
            edit_x += (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;

            if (b != '\t' && edit_pos < edit_cursor)
                edit_pos++;
        }

        if (b != '\t')
            edit_x += FONT_WIDTH * screen_scale;
    }

    edit_column = get_line_length(edit_pos);
    edit_handle_scroll();
}

static void edit_handle_up() {
    if (edit_pos == 0) return;

    edit_clear_cursor();

    int line_start = find_line_start(edit_pos);
    if (line_start == 0) {
        edit_redraw_cursor(0);
        return;
    }

    int prev_line_end = line_start - 1;
    int prev_line_start = find_line_start(prev_line_end);

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

    int next_line_start = line_end + 1;

    edit_pos = get_pos_at_column(next_line_start, edit_column);
    edit_x = get_x_from_pos(edit_pos);
    edit_y += FONT_HEIGHT * screen_scale;

    edit_handle_scroll();
}

static void edit_handle_quit() {
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
    screen_flush();
}

static void edit_handle_save() {
    file_write(edit_node, edit_buffer, edit_cursor + 1);
}

void edit_handle_type(uint8_t scancode) {
    if (scancode == KEY_ESC) return edit_handle_quit();

    if (keyboard_ctrl) {
        char c = scancode_to_char(scancode);

        if (!keyboard_shift) {
            if (c == 's') return edit_handle_save();
        }
    }

    if (scancode == KEY_ARROW_LEFT) return edit_handle_left();
    else if (scancode == KEY_ARROW_RIGHT) return edit_handle_right();
    else if (scancode == KEY_ARROW_UP) return edit_handle_up();
    else if (scancode == KEY_ARROW_DOWN) return edit_handle_down();

    char c = scancode_to_char(scancode);
    if (!c) return;

    if (c == '\b') return edit_handle_backspace();

    int max_x = screen_width - (FONT_WIDTH * screen_scale);
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

    edit_column = get_line_length(edit_pos);

    if (c == '\n') {
        redraw_from_pos(edit_pos - 1, edit_x, edit_y);
        edit_x = 0;
        edit_y += FONT_HEIGHT * screen_scale;
        edit_handle_scroll();
    } else {
        if (c == '\t') {
            for (int i = 0; i < KEYBOARD_TAB_LENGTH; i++) {
                screen_draw_char(edit_x, edit_y, ' ', EDITOR_FG, EDITOR_BG, screen_scale);
                edit_x += FONT_WIDTH * screen_scale;
            }
        } else {
            screen_draw_char(edit_x, edit_y, c, EDITOR_FG, EDITOR_BG, screen_scale);
            edit_x += FONT_WIDTH * screen_scale;
        }
        redraw_line(edit_pos, edit_x, edit_y);
        edit_redraw_cursor(0);
        edit_statusbar_draw();
    }
}