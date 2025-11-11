#include "widgets/text.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "global.h"
#include "render.hpp"

std::string Text::get() {
	return text;
}

void Text::set(std::string str, int len) {
	renderer_lock.lock();
	text = str;
	w = len;
	needRedraw = true;
	renderer_lock.unlock();
}

void Text::render(PaintBrush* pb, bool redraw) {
	if (!redraw && !needRedraw) return;
	needRedraw = false;
	pb->locate(ax, ay);
	pb->color(f, b);
	pb->text(text);
}

void Text::color(short _f, short _b) {
	renderer_lock.lock();
	if (~_f) f = _f;
	if (~_b) b = _b;
	needRedraw = true;
	renderer_lock.unlock();
}

Text::Text(int rx, int ry, int w, std::string str, int f, int b) : Widget(rx, ry, w, 1), text(str), f(f), b(b), needRedraw(true) {}