#include "widgets/widget.hpp"
#include "services/mouse.service.hpp"
#include "logger.hpp"
#include "options.hpp"

bool Widget::isFocus() {
    if (!options->mouseControlling) return false;
	auto l = mouse_service->where();
	return (ax <= l.first && l.first < ax + w) && (ay <= l.second && l.second < ay + h);	
}

void Widget::onClick(int rx, int ry) {}
void Widget::onInput(int ch) {}
void Widget::onWideClick() {}

void Widget::setabs(int x, int y) {
	ax = x + rx;
	ay = y + ry;
}

Widget::Widget(int rx, int ry, int w, int h) : rx(rx), ry(ry), w(w), h(h), ax(rx), ay(ry) {}