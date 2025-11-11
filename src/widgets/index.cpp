#include "widgets/index.hpp"
#include "render.hpp"

void Index::push(std::shared_ptr<Widget> w) {
	renderer_lock.lock();
	w->setabs(ax, ay);
	indexes.push_back(w);
    needRedraw = true;
	renderer_lock.unlock();
}

void Index::pop() {
	renderer_lock.lock();
	indexes.pop_back();
	needRedraw = true;
	renderer_lock.unlock();
}

void Index::render(PaintBrush* pb, bool redraw) {
	if (redraw || needRedraw) {
		for (auto& i : indexes) {
			i->render(pb, true);
		}
		needRedraw = false;
	}
	else {
		if (indexes.size()) indexes[indexes.size() - 1]->render(pb, false);
	}
}

void Index::onInput(int ch) {
	if (indexes.size()) {
		indexes[indexes.size() - 1]->onInput(ch);
	}
}

void Index::onClick(int ix, int iy) {
	if (indexes.size()) {
		auto& i = indexes[indexes.size() - 1];
		i->onClick(ix - i->rx, iy - i->ry);
	}
}

void Index::onWideClick() {
    if (indexes.size()) {
		auto& i = indexes[indexes.size() - 1];
		i->onWideClick();
	}
}

void Index::setabs(int x, int y) {
	ax = x + rx;
	ay = x + ry;
	for (auto& i : indexes) {
		i->setabs(ax, ay);
	}
}

Index::Index(int rx, int ry, int w, int h) : Widget(rx, ry, w, h), needRedraw(true) {}