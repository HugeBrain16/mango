#include "screen.h"
#include "font.h"
#include "string.h"
#include "color.h"
#include "heap.h"

uint32_t *screen_buffer = NULL;
uint32_t *back_buffer = NULL;
uint8_t *screen_rowdata = NULL;
size_t back_buffer_size = 0;

int screen_width = 0;
int screen_height = 0;
uint32_t screen_pitch = 0;
float screen_scale = 0;

static uint32_t *get_buffer() {
    return back_buffer ? back_buffer : screen_buffer;
}

void screen_flush() {
    size_t stride = screen_pitch / sizeof(uint32_t);
    size_t rowsize = screen_width * sizeof(uint32_t);

    if (back_buffer) {
        for (int i = 0; i < screen_height; i++) {
            if (screen_rowdata[i] & SCREEN_ROW_DIRTY) {
                size_t offset = i * stride;
                memcpy(screen_buffer + offset, back_buffer + offset, rowsize);
                screen_rowdata[i] &= ~SCREEN_ROW_DIRTY;
            }
        }
    }
}

void screen_init(multiboot_info_t *mbi) {
    screen_buffer = (uint32_t *)(uint32_t) mbi->framebuffer_addr;
    screen_width = mbi->framebuffer_width;
    screen_height = mbi->framebuffer_height;
    screen_pitch = mbi->framebuffer_pitch;
    screen_scale = 1.0f;

    screen_clear(COLOR_BLACK);
}

void screen_init_back_buffer() {
    size_t stride = screen_pitch / sizeof(uint32_t);
    back_buffer_size = stride * screen_height;
    back_buffer = heap_alloc(back_buffer_size * sizeof(uint32_t));
    memcpy(back_buffer, screen_buffer, back_buffer_size * sizeof(uint32_t));
}

void screen_init_rowdata() {
    screen_rowdata = heap_alloc(screen_height);
    memset(screen_rowdata, 0, screen_height);
}

void screen_clear(uint32_t color) {
    size_t stride = screen_pitch / sizeof(uint32_t);
    size_t pixels = stride * screen_height;

    uint32_t *buffer = get_buffer();
    for (size_t i = 0; i < pixels; i++) {
        buffer[i] = color;

        screen_rowdata[i / stride] |= SCREEN_ROW_DIRTY;
    }
}

int screen_get_pixel(int x, int y, uint32_t *color, int direct) {
    uint32_t *buffer = direct ? screen_buffer : get_buffer();

    if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) {
        *color = buffer[y * (screen_pitch / sizeof(uint32_t)) + x];
        return 1;
    }

    return 0;
}

void screen_draw_pixel(int x, int y, uint32_t color, int direct) {
    uint32_t *buffer = direct ? screen_buffer : get_buffer();

    if (x >= 0 && x < screen_width && y >= 0 && y < screen_height) {
        buffer[y * (screen_pitch / sizeof(uint32_t)) + x] = color;

        if (!direct)
            screen_rowdata[y] |= SCREEN_ROW_DIRTY;
    }
}

void screen_draw_char2(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, float scale, uint32_t *buffer) {
    const uint8_t *glyph = font_bitmaps[(unsigned char) c];
    size_t stride = screen_pitch / sizeof(uint32_t);

    int scaled_w = (int)FONT_WIDTH * scale;
    int scaled_h = (int)FONT_HEIGHT * scale;

    for (int py = 0; py < scaled_h; py++) {
        int row = (int)py / scale;
        int pixel_y = y + py;

        for (int px = 0; px < scaled_w; px++) {
            int col = (int)px / scale;
            int pixel_x = x + px;

            uint8_t line = glyph[row];        
            if (pixel_x >= 0 && pixel_x < screen_width && pixel_y >= 0 && pixel_y < screen_height) {
                if (line & (0x80 >> col))
                    buffer[pixel_y * stride + pixel_x] = fg_color;
                else if (bg_color != COLOR_TRANSPARENT)
                    buffer[pixel_y * stride + pixel_x] = bg_color;

                screen_rowdata[pixel_y] |= SCREEN_ROW_DIRTY; 
            }
        }
    }
}

void screen_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, float scale) {
    screen_draw_char2(x, y, c, fg_color, bg_color, scale, get_buffer());
}

void screen_scroll(int lines, uint32_t color) {
    size_t stride = screen_pitch / sizeof(uint32_t);
    size_t rows = screen_height - lines;

    uint32_t *buffer = get_buffer();
    memmove(buffer,
        buffer + lines * stride,
        rows * stride * sizeof(uint32_t));

    uint32_t *bottom = buffer + rows * stride;
    size_t pixels = lines * stride;
    for (size_t i = 0; i < pixels; i++)
        bottom[i] = color;

    for (int i = 0; i < screen_height; i++)
        screen_rowdata[i] |= SCREEN_ROW_DIRTY;
}

void screen_draw_rgba(const void *data, size_t size, int x, int y, int width, int height, int direct) {
    uint32_t *buffer = direct ? screen_buffer : get_buffer();
    size_t stride = screen_pitch / sizeof(uint32_t);
    uint8_t *rgba = (uint8_t *)data;

    for (int dy = 0; dy < height; dy++) {
        int py = y + dy;

        for (int dx = 0; dx < width; dx++) {
            size_t i = (dy * width + dx) * 4;

            if (i + 3 >= size)
                return;

            uint8_t r = rgba[i + 0];
            uint8_t g = rgba[i + 1];
            uint8_t b = rgba[i + 2];
            uint8_t a = rgba[i + 3];

            int px = x + dx;
            if (px < 0 || px >= screen_width || py < 0 || py >= screen_height)
                continue;

            if (a == 0)
                continue;

            uint32_t old = buffer[py * stride + px];
            uint8_t old_r = (old >> 16) & 0xff;
            uint8_t old_g = (old >> 8) & 0xff;
            uint8_t old_b = old & 0xff;

            r = (r * a + old_r * (255 - a)) / 255;
            g = (g * a + old_g * (255 - a)) / 255;
            b = (b * a + old_b * (255 - a)) / 255;

            buffer[py * stride + px] = (r << 16) | (g << 8) | b;

            if (!direct)
                screen_rowdata[py] |= SCREEN_ROW_DIRTY;
        }
    }
}
