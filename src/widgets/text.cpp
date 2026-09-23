#include "widgets/text.hpp"
#include "options.hpp"
#include <algorithm>

std::string Text::get() { return text; }

void Text::set(std::string str, int len) {
    text = str;
    firstLine = 0;
    w = len;
    needRedraw = true;
}

void Text::render(PaintBrush* pb, bool redraw) {
    if (!redraw && !needRedraw) return;
    needRedraw = false;
    pb->color(f, b);
    pb->fill(ax, ay, ax + w - 1, ay + h - 1);
    pb->locate(ax, ay);
    lineCount = pb->textBox(text, w, h, true, firstLine);
}

void Text::color(Color foreground) {
    f = foreground;
    needRedraw = true;
}

void Text::color(Color foreground, Color background) {
    f = foreground;
    b = background;
    needRedraw = true;
}

Text::Text(int rx, int ry, int w, std::string str, Color f, Color b) : Widget(rx, ry, w, 1),
    text(str),
    f(f == Color(15) ? options->colors.foreground : f),
    b(b == Color(0) ? options->colors.background : b),
    needRedraw(true) {}

bool Text::onInput(int key) {
    int next = firstLine;
    if (key == Key::PageDown) next += std::max(1, h - 1);
    else if (key == Key::PageUp) next -= std::max(1, h - 1);
    else return false;
    next = std::clamp(next, 0, std::max(0, lineCount - h));
    if (next != firstLine) {
        firstLine = next;
        needRedraw = true;
    }
    return true;
}
