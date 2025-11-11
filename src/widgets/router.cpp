#include "widgets/router.hpp"
#include "logger.hpp"
#include "render.hpp"

void Router::add(std::string name, std::shared_ptr<Widget> e) {
	e->setabs(ax, ay);
	p.insert({name, e});
}

void Router::route(std::string name) {
	if (name == current_name) return;
	renderer_lock.lock();
	if (name == "") {
		current = nullptr;
	}
	else if (p.count(name)) {
		current = p.at(name);
	}
	else {
		current = nullptr;
		logger.write(Logger::Warn, "Router Widget", "找不到路由路径 " + name);
	}
	current_name = name;
	needRedraw = true;
	renderer_lock.unlock();
}

void Router::render(PaintBrush* pb, bool redraw) {
	if (redraw || needRedraw) {
		pb->color(15, 0);
		pb->fill(ax, ay, ax + w - 1, ay + h - 1);
	}
	if (current) {
		current->render(pb, redraw || needRedraw);
		needRedraw = false;
	}
	else if (redraw || needRedraw) {
		pb->locate(ax + (w >> 1) - 4, ay + (h >> 1));
		pb->text("暂无画面");
		needRedraw = false;
	}
}

void Router::onInput(int ch) {
	if (current) current->onInput(ch);
}

void Router::onClick(int ix, int iy) {
	if (current) current->onClick(ix - current->rx, iy - current->ry);
}

void Router::onWideClick() {
    if (current) current->onWideClick();
}

void Router::setabs(int x, int y) {
	ax = x + rx;
	ay = y + ry;
	for (auto& kv : p) {
		kv.second->setabs(ax, ay);
	}
}

Router::Router(int rx, int ry, int w, int h) : Widget(rx, ry, w, h), current_name("") {
	needRedraw = false;
	p.clear();
	current = nullptr;
}