#include "widgets/datetime.hpp"
#include "i18n.hpp"
#include "options.hpp"

void DateTime::render(PaintBrush* brush, bool redraw) {
    auto now = std::time(nullptr);
    if (!redraw && now == last_check) return;
    last_check = now;
    brush->color(foreground, background);
    brush->fill(ax, ay, ax + w - 1, ay);
    brush->locate(ax, ay);
    brush->textBox(I18n::active().dateTime(now, true), w, 1, false);
}

void DateTime::color(Color front) {
    foreground = front;
    last_check = -1;
}

void DateTime::color(Color front, Color back) {
    foreground = front;
    background = back;
    last_check = -1;
}

DateTime::DateTime(int x, int y, Color front, Color back) : Widget(x, y, 56, 1),
    foreground(front == Color(15) ? options->colors.foreground : front),
    background(back == Color(0) ? options->colors.background : back) {}
