#include "widgets/widget.hpp"
#include "services/mouse.service.hpp"
#include "logger.hpp"
#include "options.hpp"

bool Widget::isFocus() const { return keyboardFocus; }

bool Widget::isFocusHighlighted() const { return keyboardFocus && focusHighlight; }

bool Widget::isHover() const {
    if (!options->term.mouse) return false;
    auto l = mouse_service->where();
    return (ax <= l.first && l.first < ax + w) && (ay <= l.second && l.second < ay + h);
}

void Widget::showFocus(bool visible) { focusHighlight = visible; }

bool Widget::focusable() const { return false; }

void Widget::focus(bool active) { keyboardFocus = active; }

void Widget::onClick(int, int) {}
bool Widget::onInput(int) { return false; }
void Widget::onWideClick() {}

void Widget::setAbs(int x, int y) {
    ax = x + rx;
    ay = y + ry;
}

Widget::Widget(int rx, int ry, int w, int h) : ax(rx), ay(ry), rx(rx), ry(ry), w(w), h(h) {}
