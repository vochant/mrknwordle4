#include "widgets/link.hpp"
#include "render.hpp"

void Link::render(PaintBrush* pb, bool redraw) {
	if (isFocus() != prevIsFocus) {
		redraw = true;
		prevIsFocus = !prevIsFocus;
	}
	if (needRedraw) {
		redraw = true;
		needRedraw = false;
	}
	if (!redraw) return;
	pb->locate(ax, ay);
	pb->color(prevIsFocus ? af : sf, prevIsFocus ? ab : sb);
	pb->text(content);
}

void Link::scolor(short _f, short _b) {
	renderer_lock.lock();
	if (~_f) sf = _f;
	if (~_b) sb = _b;
	needRedraw = true;
	renderer_lock.unlock();
}

void Link::acolor(short _f, short _b) {
	renderer_lock.lock();
	if (~_f) af = _f;
	if (~_b) ab = _b;
	needRedraw = true;
	renderer_lock.unlock();
}

std::string Link::get() {
	return content;
}

void Link::set(std::string str, int len) {
	renderer_lock.lock();
	content = str;
	w = len;
	needRedraw = true;
	renderer_lock.unlock();
}

void Link::onInput(int k) {
    if (key && k == key) {
        callback();
    }
	else if (k == '\r' && isFocus()) {
		callback();
	}
}

void Link::onClick(int ix, int iy) {
	callback();
}

Link::Link(int rx, int ry, int w, std::string str, int key, std::function<void()> callback, int sf, int sb, int af, int ab) : Widget(rx, ry, w, 1), content(str), key(key), callback(callback), sf(sf), sb(sb), af(af), ab(ab), prevIsFocus(false) {}