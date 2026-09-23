#include "widgets/button.hpp"
#include "options.hpp"

void empty_callback() {}

void Button::render(PaintBrush* pb, bool redraw) {
    const bool focused = isFocusHighlighted();
    const bool hovered = isHover();
    if (hovered != prevHover) {
        prevHover = hovered;
        redraw = true;
    }
    if (prevIsFocus != focused) {
        prevIsFocus = focused;
        if (prevIsFocus) pb->color(options->colors.selected, options->colors.background);
        else if (hovered) pb->color(options->colors.hover, options->colors.background);
        else pb->color(options->colors.foreground, options->colors.background);
        pb->rect(ax, ay, ax + w, ay + h - 1, false);
        redraw = true;
    }
    else if (redraw) {
        if (prevIsFocus) pb->color(options->colors.selected, options->colors.background);
        else if (hovered) pb->color(options->colors.hover, options->colors.background);
        else pb->color(options->colors.foreground, options->colors.background);
        pb->rect(ax, ay, ax + w, ay + h - 1, false);
    }
    if (inner) inner->render(pb, redraw);
}

bool Button::onInput(int k) {
    if ((k == '\r' && isFocus()) || (key && k == key)) {
        callback();
        return true;
    }
    return false;
}

bool Button::focusable() const { return true; }

void Button::onClick(int, int) { callback(); }

void Button::set(std::unique_ptr<Widget> w) {
    w->w = this->w - 4;
    w->h = this->h - 2;
    w->setAbs(ax + 2, ay + 1);
    inner = std::move(w);
}

void Button::setAbs(int x, int y) {
    ax = x + rx;
    ay = y + ry;
    if (inner) inner->setAbs(ax + 2, ay + 1);
}

Button::Button(int rx, int ry, int w, int h, int key, std::function<void()> callback) : Widget(rx, ry, w, h),
    callback(callback), inner(nullptr), prevIsFocus(false), prevHover(false), key(key) {}
