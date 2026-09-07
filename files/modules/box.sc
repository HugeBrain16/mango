let __box_structname = 0;

let __box_vector2d_x = 1;
let __box_vector2d_y = 2;

let __box_rect_pos = 1;
let __box_rect_w = 2;
let __box_rect_h = 3;

func box_isnumeric(val) {
	let type = type_name(val);

	return (type == "int" || type == "float");
}

func box_isstruct(val, name) {
	if (type_name(val) != "list") return false;
	if (type_name(val[__box_structname]) != "str") return false;

	return val[__box_structname] == name;
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

func box_isrect(rect) {
	if (!box_isstruct(rect, "box_rect")) return false;

	let pos = rect[__box_rect_pos];
	let w = rect[__box_rect_w];
	let h = rect[__box_rect_h];

	return box_isvector2d(pos) && box_isnumeric(w) && box_isnumeric(h);
}

func box_draw(obj, col) {
	if (box_isrect(obj)) {
		let x = obj[__box_rect_pos][__box_vector2d_x];
		let y = obj[__box_rect_pos][__box_vector2d_y];
		let w = obj[__box_rect_w];
		let h = obj[__box_rect_h];

		for (let dh = 0; dh < h; dh += 1) {
			for (let dw = 0; dw < w; dw += 1)
				screen_draw(x + dw, y + dh, col);
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