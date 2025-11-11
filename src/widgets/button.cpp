#include "widgets/button.hpp"

void emptyCallback() {}

void Button::render(PaintBrush* pb, bool redraw) {
	if (prevIsFocus != isFocus()) {
		prevIsFocus = !prevIsFocus;
		if (prevIsFocus) pb->color(14, 0);
		else pb->color(15, 0);
		pb->rect(ax, ay, ax + w - 1, ay + h - 1, false);
		redraw = true;
	}
	else if (redraw) {
		if (prevIsFocus) pb->color(14, 0);
		else pb->color(15, 0);
		pb->rect(ax, ay, ax + w - 1, ay + h - 1, false);
	}
	if (inner) inner->render(pb, redraw);
}

void Button::onInput(int k) {
	if (k == '\r' && isFocus() || key && k == key) {
		callback();
	}
}

void Button::onClick(int ix, int iy) {
	callback();
}

void Button::set(std::shared_ptr<Widget> w) {
	w->setabs(ax, ay);
	inner = w;
}

void Button::setabs(int x, int y) {
	ax = x + rx;
	ay = y + ry;
	if (inner) inner->setabs(ax + 2, ay + 1);
}

Button::Button(int rx, int ry, int w, int h, int key, std::function<void()> callback) : Widget(rx, ry, w, h), callback(callback), inner(nullptr), prevIsFocus(false), key(key) {}