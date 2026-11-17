#include <mango.h>
#include <color.h>
#include <key.h>

typedef struct {
    int x;
    int y;
} vec_t;

typedef struct {
    vec_t pos;
    int w;
    int h;
} rect_t;

static void rect_draw(rect_t *rect, uint32_t color) {
    for (int y = rect->pos.y; y < rect->pos.y + rect->h; y++) {
        for (int x = rect->pos.x; x < rect->pos.x + rect->w; x++) {
            screen_draw(x, y, color, 0);
        }
    }
}

int start() {
    int width, height;
    screen_info(&width, &height, NULL, NULL);

    rect_t box = {0};
    box.w = 10;
    box.h = 10;

    int speed = 100;
    int dx = speed;
    int dy = speed;

    int fps = 30;
    double time_last = 0;
    double time_curr = 0;
    double frame_time = 1.0 / fps;

    while (1) {
        time_curr = sys_perf();
        double dt = time_curr - time_last;
        if (dt < frame_time)
            continue;
        time_last = time_curr;

        uint8_t key = key_down();
        if (key == KEY_esc)
            break;

        screen_clear(COLOR_BLUE);

        screen_draw_char(0, 0, 'X', COLOR_BLACK, COLOR_WHITE, 1);

        box.pos.x += dx * dt;
        box.pos.y += dy * dt;

        if (box.pos.x <= 0) {
            box.pos.x = 0;
            if (dx < 0) dx = -dx;
        } else if (box.pos.x + box.w >= width) {
            box.pos.x = width - box.w;
            if (dx > 0) dx = -dx;
        }

        if (box.pos.y <= 0) {
            box.pos.y = 0;
            if (dy < 0) dy = -dy;
        } else if (box.pos.y + box.h >= height) {
            box.pos.y = height - box.h;
            if (dy > 0) dy = -dy;
        }

        rect_draw(&box, COLOR_WHITE);
        screen_flush();
    }

    screen_clear(COLOR_BLACK);
    screen_flush();
    return 0;
}
