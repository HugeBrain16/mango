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
#include "unit.h"
#include "command.h"
#include "desktop.h"

static uint32_t cursor_ticks = 0;
static int cursor_visible = 0;
static int edit_column = 0;
static int edit_showfrom = 1; // by line

static int edit_saveas = 0;
static char *edit_saveas_name = NULL;
static size_t edit_saveas_cursor = 0;
static size_t edit_saveas_pos = 0;
static int edit_saveas_y = 0;
static int edit_saveas_x = 0;

typedef struct {
    char **buffer;
    size_t *pos;
    size_t *cursor;
    int *draw_x;
    int *draw_y;
    uint32_t fg;
    uint32_t bg;
} context_t;

static context_t get_context(void) {
    if (edit_saveas) {
        return (context_t) {
            .buffer = &edit_saveas_name,
            .pos = &edit_saveas_pos,
            .cursor = &edit_saveas_cursor,
            .draw_x = &edit_saveas_x,
            .draw_y = &edit_saveas_y,
            .fg = EDITOR_STATUS_FG,
            .bg = EDITOR_STATUS_BG,
        };
    }

    return (context_t) {
        .buffer = &edit_buffer,
        .pos = &edit_pos,
        .cursor = &edit_cursor,
        .draw_x = &edit_x,
        .draw_y = &edit_y,
        .fg = EDITOR_FG,
        .bg = EDITOR_BG,
    };
}

static size_t get_block_size() {
    if (edit_node != 0) {
        file_node_t file;
        file_node(edit_node, &file);

        file_data_t block;
        file_data(file.first_block, &block);

        return sizeof(block.data);
    } else return 1;
}

static int find_line_start(int pos) {
    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    while (pos > 0 && buffer[pos - 1] != '\n')
        pos--;

    return pos;
}

static size_t find_line_end(size_t pos) {
    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    while (pos < (size_t)*ctx.cursor && buffer[pos] != '\n')
        pos++;

    return pos;
}

static int get_line_count(size_t end) {
    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    int line = 1;

    for (size_t i = 0; i < end; i++) {
        if (buffer[i] == '\n')
            line++;
    }

    return line;
}

static int get_line_length(int pos) {
    int start = find_line_start(pos);
    return pos - start;
}

static int get_x_from_pos(int pos) {
    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    int x = 0;

    int start = find_line_start(pos);
    for (int i = start; i < pos; i++) {
        char c = buffer[i];

        if (c == '\t')
            x += (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;
        else
            x += FONT_WIDTH * screen_scale;
    }

    return x;
}

static void clear(int x, int y, uint32_t color) {
    int draw_x = x;
    while (draw_x < screen_width) {
        screen_draw_char(draw_x, y, ' ', color, color, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
    }
}

static void redraw_line(int start_pos, int x, int y) {
    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    size_t i = start_pos;

    while (i < (size_t)*ctx.cursor && buffer[i] != '\n') {
        char c = buffer[i];

        if (c == '\t') {
            for (int j = 0; j < KEYBOARD_TAB_LENGTH; j++) {
                screen_draw_char(x, y, ' ', ctx.fg, ctx.bg, screen_scale);
                x += FONT_WIDTH * screen_scale;
            }
        } else {
            screen_draw_char(x, y, c, ctx.fg, ctx.bg, screen_scale);
            x += FONT_WIDTH * screen_scale;
        }

        i++;
    }
}

static void redraw_from_pos(int start_pos, int x, int y) {
    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    size_t i = start_pos;

    while (i < (size_t)*ctx.cursor) {
        char c = buffer[i];

        if (c != '\n') {
            if (c == '\t')
                for (int j = 0; j < KEYBOARD_TAB_LENGTH; j++) {
                    screen_draw_char(x, y, ' ', ctx.fg, ctx.bg, screen_scale);
                    x += FONT_WIDTH * screen_scale;
                }
            else {
                screen_draw_char(x, y, c, ctx.fg, ctx.bg, screen_scale);
                x += FONT_WIDTH * screen_scale;
            }
        } else {
            clear(x, y, ctx.bg);
            x = 0;
            y += FONT_HEIGHT * screen_scale;
        }

        i++;
    }

    clear(x, y, ctx.bg);

    y += FONT_HEIGHT * screen_scale;
    clear(0, y, ctx.bg);
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
    keyboard_mode = KEYBOARD_MODE_EDIT;
    screen_clear(EDITOR_BG);

    edit_saveas = 0;

    edit_node = file_sector;
    if (file_sector != 0)
        edit_buffer = file_read(file_sector);
    else {
        edit_buffer = heap_alloc(1);
        memset(edit_buffer, 0, 1);
    }
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
    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    if (*ctx.pos < *ctx.cursor) {
        char c = buffer[*ctx.pos];

        if (c == '\n' || c == '\t')
            screen_draw_char(*ctx.draw_x, *ctx.draw_y, ' ', ctx.fg, ctx.bg, screen_scale);
        else
            screen_draw_char(*ctx.draw_x, *ctx.draw_y, c, ctx.fg, ctx.bg, screen_scale);
    } else
        screen_draw_char(*ctx.draw_x, *ctx.draw_y, ' ', ctx.fg, ctx.bg, screen_scale);
}

void edit_draw_cursor() {
    uint32_t cursor_delay = ms_to_ticks(EDITOR_CURSOR_BLINK);

    if (pit_ticks - cursor_ticks >= cursor_delay) {
        cursor_visible = !cursor_visible;
        cursor_ticks = pit_ticks;

        edit_clear_cursor();

        if (cursor_visible) {
            context_t ctx = get_context();
            screen_draw_char(*ctx.draw_x, *ctx.draw_y, '_', ctx.fg, COLOR_TRANSPARENT, screen_scale);
        }

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
    if (edit_node != 0)
        file_node(edit_node, &file);

    int draw_x = 0;
    int draw_y = screen_height - (FONT_HEIGHT * screen_scale);

    clear(draw_x, draw_y, EDITOR_STATUS_BG);

    char status[128];
    if (edit_saveas) {
        strfmt(status, "Save as: %s", edit_saveas_name ? edit_saveas_name : "");
    } else {
        strfmt(status, "%s | Line %d-%d | Chars %d | Pos %d | Save: Ctrl+s, Quit: Esc",
            edit_node != 0 ? file.name : "Buffer",
            get_line_count(edit_pos),
            get_line_count(edit_cursor),
            edit_cursor,
            edit_pos);
    }

    draw_x = 0;
    for (const char *p = status; *p != '\0'; p++) {
        screen_draw_char(draw_x, draw_y, *p, EDITOR_STATUS_FG, COLOR_TRANSPARENT, screen_scale);
        draw_x += FONT_WIDTH * screen_scale;
    }

    if (edit_saveas) {
        const int prefix_len = 9;
        edit_saveas_x = (prefix_len + edit_saveas_pos) * (FONT_WIDTH * screen_scale);
        edit_saveas_y = draw_y;

        edit_clear_cursor();
    }

    screen_flush();
}

static void edit_handle_scroll() {
    if (edit_saveas) {
        edit_statusbar_draw();
        return;
    }

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
    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    if (*ctx.pos == 0) return;

    edit_clear_cursor();

    char deleted = buffer[*ctx.pos - 1];

    for (size_t i = *ctx.pos - 1; i < (size_t)*ctx.cursor; i++)
        buffer[i] = buffer[i + 1];

    buffer[--(*ctx.cursor)] = '\0';
    (*ctx.pos)--;

    if (deleted == '\n') {
        *ctx.draw_y -= FONT_HEIGHT * screen_scale;
        *ctx.draw_x = get_x_from_pos(*ctx.pos);
        redraw_from_pos(*ctx.pos, *ctx.draw_x, *ctx.draw_y);
    } else {
        clear(0, *ctx.draw_y, ctx.bg);
        redraw_line(find_line_start(*ctx.pos), 0, *ctx.draw_y);

        if (deleted == '\t')
            *ctx.draw_x -= (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;
        else
            *ctx.draw_x -= FONT_WIDTH * screen_scale;
    }

    if (!edit_saveas)
        edit_column = get_line_length(edit_pos);
    edit_handle_scroll();
}

static void edit_handle_left() {
    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    if (*ctx.pos == 0) return;

    edit_clear_cursor();
    (*ctx.pos)--;

    char c = buffer[*ctx.pos];
    if (c == '\n') {
        *ctx.draw_y -= FONT_HEIGHT * screen_scale;
        *ctx.draw_x = get_x_from_pos(*ctx.pos);
    } else {
        if (c == '\t')
            *ctx.draw_x -= (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;
        else if (*ctx.draw_x > 0)
            *ctx.draw_x -= FONT_WIDTH * screen_scale;
        else
            *ctx.draw_x = 0;
    }

    if (!edit_saveas)
        edit_column = get_line_length(edit_pos);
    edit_handle_scroll();
}

static void edit_handle_right() {
    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    if (*ctx.pos == *ctx.cursor) return;

    edit_clear_cursor();

    /* increment inside branch so shifting between lines isnt weird */
    if (buffer[*ctx.pos] == '\n') {
        (*ctx.pos)++;
        *ctx.draw_y += FONT_HEIGHT * screen_scale;
        *ctx.draw_x = 0;
    } else {
        char b = buffer[*ctx.pos];
        (*ctx.pos)++;
        char c = buffer[*ctx.pos];

        if (c == '\t' || b == '\t') {
            *ctx.draw_x += (FONT_WIDTH * screen_scale) * KEYBOARD_TAB_LENGTH;

            if (b != '\t' && *ctx.pos < *ctx.cursor)
                (*ctx.pos)++;
        }

        if (b != '\t')
            *ctx.draw_x += FONT_WIDTH * screen_scale;
    }

    if (!edit_saveas)
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

    if (desktop_active) {
        desktop_init();
    } else {
        term_init(TERM_DRAW_DEFAULT);
    }
}

static void edit_handle_save() {
    if (edit_node != 0)
        file_write(edit_node, edit_buffer, edit_cursor + 1);
    else {
        size_t size = FILE_MAX_NAME + FILE_MAX_PATH;
        edit_saveas_name = heap_alloc(size);
        memset(edit_saveas_name, 0, size);

        edit_saveas = 1;
        edit_saveas_pos = 0;
        edit_saveas_cursor = 0;
        edit_saveas_x = 0;
        edit_saveas_y = 0;

        edit_statusbar_draw();
    }
}

void edit_handle_type(uint8_t scancode) {
    if (scancode == KEY_ESC) {
        if (edit_saveas) {
            edit_saveas = 0;
            if (edit_saveas_name) {
                heap_free(edit_saveas_name);
                edit_saveas_name = NULL;
            }
            edit_saveas_pos = 0;
            edit_saveas_cursor = 0;
            edit_statusbar_draw();
            return;
        } else
            return edit_handle_quit();
    }

    if (keyboard_ctrl && !edit_saveas) {
        char c = scancode_to_char(scancode);

        if (!keyboard_shift) {
            if (c == 's') return edit_handle_save();
        }
    }

    if (scancode == KEY_ARROW_LEFT) return edit_handle_left();
    else if (scancode == KEY_ARROW_RIGHT) return edit_handle_right();
    else if (scancode == KEY_ARROW_UP && !edit_saveas) return edit_handle_up();
    else if (scancode == KEY_ARROW_DOWN && !edit_saveas) return edit_handle_down();

    char c = scancode_to_char(scancode);
    if (!c) return;

    if (c == '\b') return edit_handle_backspace();

    if (edit_saveas && c == '\n') {
        char *cmd = heap_alloc(strlen(edit_saveas_name) + 16);
        strfmt(cmd, "newfile %s", edit_saveas_name);
        command_handle(cmd, 0);
        heap_free(cmd);

        uint32_t node = file_get_node(edit_saveas_name);
        if (node) {
            edit_node = node;
            edit_saveas = 0;
            if (edit_saveas_name) {
                heap_free(edit_saveas_name);
                edit_saveas_name = NULL;
            }
            edit_saveas_pos = 0;
            edit_saveas_cursor = 0;
            edit_handle_save();
        }

        edit_statusbar_draw();
        return;
    }

    context_t ctx = get_context();
    char *buffer = *ctx.buffer;

    int max_x = screen_width - (FONT_WIDTH * screen_scale);
    if (*ctx.draw_x >= max_x && c != '\n') return;

    edit_clear_cursor();

    if (!edit_saveas && buffer[*ctx.cursor] != '\0') {
        size_t old_size = *ctx.cursor + 1;
        size_t new_size = old_size + get_block_size();
        buffer = heap_realloc(buffer, new_size);
        *ctx.buffer = buffer; 

        for (size_t i = old_size; i < new_size; i++)
            buffer[i] = '\0';
    }

    for (size_t i = *ctx.cursor; i > (size_t)*ctx.pos; i--)
        buffer[i] = buffer[i - 1];

    buffer[*ctx.pos] = c;
    (*ctx.cursor)++;
    (*ctx.pos)++;

    if (!edit_saveas)
        edit_column = get_line_length(edit_pos);

    if (c == '\n') {
        redraw_from_pos(edit_pos - 1, edit_x, edit_y);
        edit_x = 0;
        edit_y += FONT_HEIGHT * screen_scale;
        edit_handle_scroll();
    } else {
        if (c == '\t' && !edit_saveas) {
            for (int i = 0; i < KEYBOARD_TAB_LENGTH; i++) {
                screen_draw_char(edit_x, edit_y, ' ', EDITOR_FG, EDITOR_BG, screen_scale);
                edit_x += FONT_WIDTH * screen_scale;
            }
        } else {
            screen_draw_char(*ctx.draw_x, *ctx.draw_y, c, ctx.fg, ctx.bg, screen_scale);
            *ctx.draw_x += FONT_WIDTH * screen_scale;
        }
        redraw_line(*ctx.pos, *ctx.draw_x, *ctx.draw_y);
        edit_redraw_cursor(0);
        edit_statusbar_draw();
    }
}