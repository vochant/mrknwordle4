#include "widgets/switch.hpp"
#include "render.hpp"

void emptyCallbackWithBoolean(bool v) {}

void Switch::render(PaintBrush* pb, bool redraw) {
	if (prevIsFocus != isFocus()) {
		prevIsFocus = !prevIsFocus;
		redraw = true;
	}
	redraw |= needRedraw;
	needRedraw = false;
	if (!redraw) return;
	pb->locate(ax, ay);
	pb->color(prevIsFocus ? 14 : 15, 0);
	pb->text(status ? "●True" : "○False");
}

bool Switch::get() {
	return status;
}

void Switch::onInput(int k) {
	if (k == '\r' && isFocus()) {
		renderer_lock.lock();
		status = !status;
		needRedraw = true;
		callback(status);
		renderer_lock.unlock();
	}
}

void Switch::onClick(int ix, int iy) {
	renderer_lock.lock();
	status = !status;
	needRedraw = true;
	callback(status);
	renderer_lock.unlock();
}

Switch::Switch(int rx, int ry, std::function<void(bool)> callback, bool Init) : status(Init), Widget(rx, ry, 7, 1), needRedraw(true), prevIsFocus(false), callback(callback) {}