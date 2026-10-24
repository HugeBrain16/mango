include "box.sc";

let vec = box_vector2d;

let COLOR_BG = 0;
let COLOR_FG = 0;

let WIDTH = screen_width();
let HEIGHT = screen_height();

let text = null;
let text_scale = 2;
let text_width = 0;
let text_height = 0;

let text_fps = null;
let text_fps_color = color_rgb(255, 255, 255);

let x = 0;
let y = 0;
let speed = 100;
let dx = speed;
let dy = speed;

let fps = 0;
let fps_last = sys_perf();

func randomize_color() {
    COLOR_FG = color_rgb(randrange(1, 255), randrange(1, 255), randrange(1, 255));
}

func get_width(text) {
    let w = 0;
    let fw = font_width();

    for (let i = 0; i < sizeof(text[__box_text_text]); i += 1)
        w += as_int(fw * text_scale);

    return w;
}

func init() {
    screen_init();

    text = box_text(vec(x, y), "Hello Box!", text_scale, COLOR_BG);
    text_width = get_width(text);
    text_height = font_height() * text_scale;

    text_fps = box_text(vec(0, 0), "FPS: " + as_str(fps), 1, COLOR_BG);

    randomize_color();
}

func update(dt) {
    x += dx * dt;
    y += dy * dt;

    let old_dx = dx;
    let old_dy = dy;

    if (x <= 0) {
        x = 0;
        if (dx < 0) dx = -dx;
    } else if (x + text_width >= WIDTH) {
        x = WIDTH - text_width;
        if (dx > 0) dx = -dx;
    }

    if (y <= 0) {
        y = 0;
        if (dy < 0) dy = -dy;
    } else if (y + text_height >= HEIGHT) {
        y = HEIGHT - text_height;
        if (dy > 0) dy = -dy;
    }

    if (old_dx != dx || old_dy != dy)
        randomize_color();

    list_set(text[__box_text_pos], __box_vector2d_x, x);
    list_set(text[__box_text_pos], __box_vector2d_y, y);
}

func draw() {
    screen_clear(COLOR_BG);
    box_draw(text, COLOR_FG);
    box_draw(text_fps, text_fps_color);
    screen_flush();

    fps += 1;
    let fps_now = sys_perf();
    if (fps_now - fps_last >= 1) {
        list_set(text_fps, __box_text_text, "FPS: " + as_str(fps));
        fps = 0;
        fps_last = fps_now;
    }
}

box_fps(30);
box_run(init, update, draw);