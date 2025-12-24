#include "screen.h"
#include "font.h"

void screen_init(multiboot_info_t *mbi) {
    screen_buffer = (uint32_t *)(uint32_t) mbi->framebuffer_addr;
    screen_width = mbi->framebuffer_width;
    screen_height = mbi->framebuffer_height;
    screen_pitch = mbi->framebuffer_pitch;
}

void screen_draw_char(int x, int y, char c, uint32_t fg_color, uint32_t bg_color, int scale) {
    const uint8_t *glyph = font_bitmaps[(int) c];

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t line = glyph[row];

        for (int col = 0; col < FONT_WIDTH; col++) {
            for (int scale_y = 0; scale_y < scale; scale_y++) {
                for (int scale_x = 0; scale_x < scale; scale_x++) {
                    int pixel_x = x + (col * scale) + scale_x;
                    int pixel_y = y + (row * scale) + scale_y;

                    if (line & (0x80 >> col))
                        screen_buffer[pixel_y * screen_width + pixel_x] = fg_color;
                    else
                        screen_buffer[pixel_y * screen_width + pixel_x] = bg_color;
                }
            }
        }
    }
}
