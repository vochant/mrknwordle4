#include "widgets/switch.hpp"
#include "options.hpp"
#include <utility>

void empty_bool_callback(bool) {}

void Switch::render(PaintBrush* pb, bool redraw) {
    const bool focused = isFocusHighlighted();
    const bool hovered = isHover();
    if (hovered != prevHover) {
        prevHover = hovered;
        redraw = true;
    }
    if (prevIsFocus != focused) {
        prevIsFocus = focused;
        redraw = true;
    }
    redraw |= needRedraw;
    needRedraw = false;
    if (!redraw) return;
    pb->locate(ax, ay);
    pb->color(
        prevIsFocus ?
        options->colors.selected : (
            hovered ?
            options->colors.hover :
            options->colors.foreground
        ),
        options->colors.background
    );
    const auto symbol = term ? term->prepareGlyph(status ? "●" : "○") : prepare_glyph(status ? "●" : "○");
    const std::string marker = symbol.replaced ? (status ? "+" : "-") : symbol.text;
    pb->textBox(marker + " " + (status ? onText : offText), w, 1, false);
}

bool Switch::get() const { return status; }

void Switch::set(bool value) {
    if (status == value) return;
    status = value;
    needRedraw = true;
}

bool Switch::onInput(int k) {
    if (key && k == key) {
        status = !status;
        needRedraw = true;
        callback(status);
        return true;
    }
    return false;
}

bool Switch::focusable() const { return false; }

void Switch::onClick(int, int) {
    status = !status;
    needRedraw = true;
    callback(status);
}

Switch::Switch(
    int rx, int ry, int w, std::string offText, std::string onText, int key, std::function<void(bool)> callback,
    bool Init
) : Widget(rx, ry, w, 1),
    status(Init), needRedraw(true), prevIsFocus(false), prevHover(false),
    key(key), callback(std::move(callback)),
    offText(std::move(offText)), onText(std::move(onText)) {}
