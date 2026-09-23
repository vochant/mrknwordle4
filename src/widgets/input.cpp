#include "widgets/input.hpp"
#include "options.hpp"

#include <algorithm>

namespace {
    std::string join(const std::vector<std::string>& clusters, size_t count) {
        std::string res;
        for (size_t i = 0; i < std::min(count, clusters.size()); i++) res += clusters[i];
        return res;
    }
} // namespace

bool accept_all(char32_t ch) { return ch >= 32 && ch != 127 && ch <= 0x10ffff; }

bool accept_number(char32_t ch) { return ch >= '0' && ch <= '9'; }

bool accept_identifier(char32_t ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') || ch == '_';
}

int Input::glyphWidth(size_t index) const {
    if (password) return 1;
    auto glyph = term ? term->prepareGlyph(content[index]) : prepare_glyph(content[index]);
    return glyph.width;
}

void Input::insert(char32_t point) {
    auto encoded = encode_utf8(point);
    if (encoded.empty()) return;
    content.insert(content.begin() + cursor, encoded);
    auto before = join(content, cursor + 1);
    auto value = join(content, content.size());
    content = split_graphemes(value);
    cursor = split_graphemes(before).size();
}

void Input::render(PaintBrush* brush, bool redraw) {
    bool focused = isFocus();
    const bool hovered = isHover();
    if (hovered != prevHover) {
        prevHover = hovered;
        redraw = true;
    }
    redraw |= changed || focused != this->focused;
    changed = false;
    this->focused = focused;
    if (!redraw) return;

    brush->color(
        active ?
        options->colors.inputActive : (
            hovered ?
            options->colors.hover :
            options->colors.foreground
        ),
        options->colors.background
    );
    brush->rect(ax, ay, ax + w, ay + 2, true);
    int width = std::max(0, w - 4);
    if (!width) return;
    brush->locate(ax + 2, ay + 1);

    if (content.empty() && !active) {
        brush->color(options->colors.inputPlaceholder, options->colors.background);
        brush->textBox(placeholder, width, 1, false);
        return;
    }

    if (cursor < scroll) scroll = cursor;
    int before = 0;
    for (size_t index = scroll; index < cursor; ++index) before += glyphWidth(index);
    int cursorWidth = cursor < content.size() ? glyphWidth(cursor) : 1;
    while (scroll < cursor && before + (active ? cursorWidth : 0) > width) {
        before -= glyphWidth(scroll++);
    }
    while (scroll > 0 && before + glyphWidth(scroll - 1) + (active ? cursorWidth : 0) <= width) {
        before += glyphWidth(--scroll);
    }

    int column = 0;
    brush->color(options->colors.foreground, options->colors.background);
    for (size_t index = scroll; index < content.size(); ++index) {
        int currentWidth = glyphWidth(index);
        if (column + currentWidth > width) break;
        if (active && index == cursor) {
            brush->color(
                Color(options->colors.foreground.value, options->colors.foreground.styles | Reverse),
                options->colors.background
            );
        }
        brush->text(password ? "*" : content[index]);
        if (active && index == cursor) brush->color(options->colors.foreground, options->colors.background);
        column += currentWidth;
    }
    if (active && cursor == content.size() && column < width) {
        brush->color(
            Color(options->colors.foreground.value, options->colors.foreground.styles | Reverse),
            options->colors.background
        );
        brush->single(' ');
    }
}

bool Input::onInput(int key) {
    if (!isFocus()) return false;
    if (key == '\t') { return false; }
    if (key == Key::Left || key == Key::CtrlLeft) {
        cursor = key == Key::CtrlLeft ? 0 : cursor ? cursor - 1 : 0;
        changed = true;
        return true;
    }
    if (key == Key::Right || key == Key::CtrlRight) {
        cursor = key == Key::CtrlRight ? content.size() : std::min(cursor + 1, content.size());
        changed = true;
        return true;
    }
    if (key == Key::Home) {
        cursor = 0;
        changed = true;
        return true;
    }
    if (key == Key::End) {
        cursor = content.size();
        changed = true;
        return true;
    }
    if (key == '\b') {
        if (cursor) content.erase(content.begin() + --cursor);
        changed = true;
        return true;
    }
    if (key == Key::Delete) {
        if (cursor < content.size()) content.erase(content.begin() + cursor);
        changed = true;
        return true;
    }
    if (key == '\r') {
        if (submit) submit();
        return submit != nullptr;
    }
    if (key == Key::Escape || key < 0 || key > 0x10ffff) return false;
    if (filter(key)) {
        insert(key);
        changed = true;
        return true;
    }
    return false;
}

void Input::onClick(int x, int) {
    changed = true;
    int target = std::max(0, x - 2), column = 0;
    cursor = scroll;
    while (cursor < content.size()) {
        int width = glyphWidth(cursor);
        if (column + width > target) break;
        column += width;
        cursor++;
    }
}

void Input::onWideClick() {}

bool Input::focusable() const { return true; }

void Input::focus(bool value) {
    if (active == value && keyboardFocus == value) return;
    active = value;
    keyboardFocus = value;
    changed = true;
}

std::string Input::get() const { return join(content, content.size()); }

void Input::set(std::string value) {
    content = split_graphemes(value);
    cursor = content.size();
    scroll = 0;
    changed = true;
}

void Input::setSubmit(std::function<void()> callback) { submit = std::move(callback); }

Input::Input(
    int rx, int ry, int w,
    bool password, std::string value, std::string placeholder,
    InputFilter filter, bool active
) : Widget(rx, ry, w, 3),
    content(split_graphemes(value)), placeholder(std::move(placeholder)),
    filter(std::move(filter)), cursor(content.size()),
    active(active), password(password) {}
