#include "widgets/layout.hpp"

void no_frame(int x, int y, int w, int h, PaintBrush* pb) {}

void single_frame(int x, int y, int w, int h, PaintBrush* pb) {
	pb->color(15, 0);
	pb->rect(x, y, x + w - 1, y + h - 1, true);
}

void Layout::render(PaintBrush* pb, bool redraw) {
	if (redraw) {
		fd(ax, ay, w, h, pb);
	}
	for (auto& i : items) {
		i->render(pb, redraw);
	}
}

void Layout::add(std::shared_ptr<Widget> w) {
	w->setabs(ax, ay);
	items.push_back(w);
}

void Layout::onInput(int ch) {
	for (auto& i : items) {
		i->onInput(ch);
	}
}

void Layout::onClick(int ix, int iy) {
	for (auto& i : items) {
		if ((i->rx <= ix && ix < i->rx + i->w) && (i->ry <= iy && iy < i->ry + i->h)) i->onClick(ix - i->rx, iy - i->ry);
        else i->onWideClick();
	}
}

void Layout::onWideClick() {
    for (auto& i : items) i->onWideClick();
}

void Layout::setabs(int x, int y) {
	ax = x + rx;
	ay = y + ry;
	for (auto& i : items) {
		i->setabs(ax, ay);
	}
}

Layout::Layout(int rx, int ry, int w, int h, FrameDrawer fd) : Widget(rx, ry, w, h), fd(fd) {}