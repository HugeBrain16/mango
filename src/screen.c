#include "screen.h"
#include "font.h"
#include "string.h"
#include "color.h"

void screen_init(multiboot_info_t *mbi) {
    screen_buffer = (uint32_t *)(uint32_t) mbi->framebuffer_addr;
    screen_width = mbi->framebuffer_width;
    screen_height = mbi->framebuffer_height;
    screen_pitch = mbi->framebuffer_pitch;
    screen_scale = 1.0f;

    screen_clear(COLOR_BLACK);
}

void screen_clear(uint32_t color) {
    size_t stride = screen_pitch / sizeof(uint32_t);
    size_t pixels = stride * screen_height;

    for (size_t i = 0; i < pixels; i++)
        screen_buffer[i] = color;
}

void screen_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, float scale) {
    const uint8_t *glyph = font_bitmaps[(unsigned char) c];

    int scaled_w = (int)FONT_WIDTH * scale;
    int scaled_h = (int)FONT_HEIGHT * scale;

    for (int py = 0; py < scaled_h; py++) {
        for (int px = 0; px < scaled_w; px++) {
            int col = (int)px / scale;
            int row = (int)py / scale;

            uint8_t line = glyph[row];

            int pixel_x = x + px;
            int pixel_y = y + py;
        
            if (line & (0x80 >> col))
                screen_buffer[pixel_y * (screen_pitch / sizeof(uint32_t)) + pixel_x] = fg_color;
            else if (bg_color != COLOR_TRANSPARENT)
                screen_buffer[pixel_y * (screen_pitch / sizeof(uint32_t)) + pixel_x] = bg_color;
        }
    }
}

void screen_scroll(int lines) {
    size_t stride = screen_pitch / sizeof(uint32_t);
    size_t rows = screen_height - lines;

    memmove(screen_buffer,
        screen_buffer + lines * stride,
        rows * stride * sizeof(uint32_t));

    uint32_t *bottom = screen_buffer + rows * stride;
    size_t pixels = lines * stride;
    for (size_t i = 0; i < pixels; i++)
        bottom[i] = COLOR_BLACK;
}
