include "struct.sc";

let box_isstruct = struct_isstruct;

let __box_vector2d_x = 1;
let __box_vector2d_y = 2;

let __box_rect_pos = 1;
let __box_rect_w = 2;
let __box_rect_h = 3;

let __box_text_pos = 1;
let __box_text_text = 2;
let __box_text_scale = 3;
let __box_text_bg = 4;

let __box_fps = 30;
let __box_timer_last = 0;
let __box_timer_current = 0;

func box_isnumeric(val) {
	let type = type_name(val);

	return (type == "int" || type == "float");
}

func box_vector2d(x, y) {
	let vec = list_init();
	list_push(vec, "box_vector2d");
	list_push(vec, x);
	list_push(vec, y);
	return vec;
}

func box_isvector2d(vec) {
	if (!box_isstruct(vec, "box_vector2d")) return false;

	let x = vec[__box_vector2d_x];
	let y = vec[__box_vector2d_y];

	return box_isnumeric(x) && box_isnumeric(y);
}

func box_rect(pos, w, h) {
	if (!box_isvector2d(pos) || !box_isnumeric(w) || !box_isnumeric(h))
		return;

	let rect = list_init();
	list_push(rect, "box_rect");
	list_push(rect, pos);
	list_push(rect, w);
	list_push(rect, h);
	return rect;
}

func box_text(pos, text, scale, bg) {
	if (
		!box_isvector2d(pos) ||
		type_name(text) != "str" ||
		!box_isnumeric(scale) ||
		!box_isnumeric(bg)
	)
		return;
	
	let txt = list_init();
	list_push(txt, "box_text");
	list_push(txt, pos);
	list_push(txt, text);
	list_push(txt, scale);
	list_push(txt, bg);
	return txt;
}

func box_isrect(rect) {
	if (!box_isstruct(rect, "box_rect")) return false;

	let pos = rect[__box_rect_pos];
	let w = rect[__box_rect_w];
	let h = rect[__box_rect_h];

	return box_isvector2d(pos) && box_isnumeric(w) && box_isnumeric(h);
}

func box_istext(txt) {
	if (!box_isstruct(txt, "box_text")) return false;

	let pos = txt[__box_text_pos];
	let text = txt[__box_text_text];
	let scale = txt[__box_text_scale];
	let bg = txt[__box_text_bg];

	return 
		box_isvector2d(pos) &&
		type_name(text) == "str" &&
		box_isnumeric(scale) &&
		box_isnumeric(bg)
	;
}

func box_draw(obj, col) {
	if (box_isrect(obj)) {
		let x = obj[__box_rect_pos][__box_vector2d_x];
		let y = obj[__box_rect_pos][__box_vector2d_y];
		let w = obj[__box_rect_w];
		let h = obj[__box_rect_h];

		for (let dh = 0; dh < h; dh += 1) {
			for (let dw = 0; dw < w; dw += 1)
				screen_draw(as_int(x) + dw, as_int(y) + dh, col);
		}
	} else if (box_istext(obj)) {
		let x = obj[__box_text_pos][__box_vector2d_x];
		let y = obj[__box_text_pos][__box_vector2d_y];
		let text = obj[__box_text_text];
		let scale = obj[__box_text_scale];
		let bg = obj[__box_text_bg];
		let fw = font_width();

		for (let i = 0; i < sizeof(text); i += 1) {
			screen_draw_char(as_int(x), as_int(y), as_char(text[i]), col, bg, as_float(scale));
			x += as_int(fw * scale);
		}
	}
}

func box_rect_colliding(rect1, rect2) {
	let x1 = rect1[__box_rect_pos][__box_vector2d_x];
	let y1 = rect1[__box_rect_pos][__box_vector2d_y];
	let w1 = rect1[__box_rect_w];
	let h1 = rect1[__box_rect_h];

	let x2 = rect2[__box_rect_pos][__box_vector2d_x];
	let y2 = rect2[__box_rect_pos][__box_vector2d_y];
	let w2 = rect2[__box_rect_w];
	let h2 = rect2[__box_rect_h];

	return
		x1 < x2 + w2 &&
		x1 + w1 > x2 &&
		y1 < y2 + h2 &&
		y1 + h1 > y2;
}

func box_fps(fps) {
	if (type_name(fps) == "int")
		__box_fps = fps;
}

func box_run(init, update, draw) {
	init();
	__box_timer_last = sys_perf();

	let frame_time = 1 / __box_fps;
	while (true) {
		__box_timer_current = sys_perf();
		let dt = __box_timer_current - __box_timer_last;

		if (dt < frame_time)
			continue;

		__box_timer_last = __box_timer_current;
		update(dt);
		draw();
	}
}