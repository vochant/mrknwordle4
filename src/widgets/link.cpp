#include "widgets/link.hpp"
#include "options.hpp"
#include <algorithm>

void Link::render(PaintBrush* pb, bool redraw) {
    const bool focused = isFocusHighlighted();
    const bool hovered = isHover();
    if (hovered != prevHover) {
        prevHover = hovered;
        redraw = true;
    }
    if (focused != prevIsFocus) {
        redraw = true;
        prevIsFocus = focused;
    }
    if (needRedraw) {
        redraw = true;
        needRedraw = false;
    }
    if (!redraw) return;
    pb->color(
        hovered ? options->colors.hover : (prevIsFocus ? af : sf),
        hovered ? options->colors.background : (prevIsFocus ? ab : sb)
    );
    const int clearWidth = std::max(w, renderedWidth);
    if (clearWidth > 0) pb->fill(ax, ay, ax + clearWidth - 1, ay);
    renderedWidth = w;
    pb->locate(ax, ay);
    pb->textBox(content, w, 1, false);
}

void Link::scolor(Color foreground) {
    sf = foreground;
    needRedraw = true;
}

void Link::scolor(Color foreground, Color background) {
    sf = foreground;
    sb = background;
    needRedraw = true;
}

void Link::acolor(Color foreground) {
    af = foreground;
    needRedraw = true;
}

void Link::acolor(Color foreground, Color background) {
    af = foreground;
    ab = background;
    needRedraw = true;
}

std::string Link::get() { return content; }

void Link::set(std::string str, int len) {
    content = str;
    w = len;
    needRedraw = true;
}

bool Link::onInput(int k) {
    if (key && k == key) {
        callback();
        return true;
    }
    return false;
}

bool Link::focusable() const { return false; }

void Link::onClick(int, int) { callback(); }

Link::Link(
    int rx, int ry, int w,
    std::string str,
    int key,
    std::function<void()> callback,
    Color sf, Color sb, Color af,
    Color ab
) : Widget(rx, ry, w, 1),
    sf(sf == Color(15) ? options->colors.foreground : sf),
    sb(sb == Color(0) ? options->colors.background : sb),
    af(af == Color(14) ? options->colors.selected : af),
    ab(ab == Color(0) ? options->colors.background : ab),
    key(key), content(str), callback(callback),
    prevIsFocus(false), prevHover(false) {}
