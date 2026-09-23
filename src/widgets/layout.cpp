#include "widgets/layout.hpp"
#include "options.hpp"
#include <algorithm>

void no_frame(int, int, int, int, PaintBrush*) {}

void single_frame(int x, int y, int w, int h, PaintBrush* pb) {
    pb->color(options->colors.foreground, options->colors.background);
    pb->rect(x, y, x + w, y + h - 1, true);
}

void Layout::render(PaintBrush* pb, bool redraw) {
    if (redraw) fd(ax, ay, w, h, pb);
    bool sthHovered = false;
    for (const auto& item : items) {
        if (item->focusable() && item->isHover()) {
            sthHovered = true;
            break;
        }
    }
    for (auto& i : items) i->showFocus(!sthHovered || i->isHover());
    for (auto& i : items) { i->render(pb, redraw); }
}

void Layout::add(std::unique_ptr<Widget> w) {
    w->setAbs(ax, ay);
    if (!hasFocused && w->focusable()) {
        focused = items.size();
        w->focus(true);
        hasFocused = true;
    }
    items.push_back(std::move(w));
}

bool Layout::onInput(int ch) {
    if (inputHandler && inputHandler(ch)) return true;
    if (ch == '\t') {
        const bool hasFocusable = std::any_of(items.begin(), items.end(), [](const auto& item) {
            return item->focusable();
        });
        if (!hasFocusable) {
            for (auto& item : items) {
                if (item->onInput(ch)) return true;
            }
            return false;
        }
        if (hasFocused && focused < items.size() && items[focused]->onInput(ch)) return true;
        focusNext();
        return true;
    }
    for (auto& i : items) {
        if (i->onInput(ch)) return true;
    }
    if (ch == Key::Down) {
        focusNext();
        return true;
    }
    if (ch == Key::Up) {
        focusPrev();
        return true;
    }
    return false;
}

void Layout::setInput(std::function<bool(int)> handler) { inputHandler = std::move(handler); }

void Layout::focusNext() {
    if (items.empty()) return;
    for (size_t offset = 0; offset < items.size(); ++offset) {
        size_t index = hasFocused ? (focused + offset + 1) % items.size() : offset;
        if (!items[index]->focusable()) continue;
        if (hasFocused) items[focused]->focus(false);
        focused = index;
        items[focused]->focus(true);
        hasFocused = true;
        return;
    }
}

void Layout::focusPrev() {
    if (items.empty()) return;
    for (size_t offset = 0; offset < items.size(); ++offset) {
        size_t index = hasFocused ? (focused + items.size() - offset - 1) % items.size() : items.size() - offset - 1;
        if (!items[index]->focusable()) continue;
        if (hasFocused) items[focused]->focus(false);
        focused = index;
        items[focused]->focus(true);
        hasFocused = true;
        return;
    }
}

void Layout::clearFocus() {
    if (!hasFocused) return;
    items[focused]->focus(false);
    hasFocused = false;
}

void Layout::onClick(int ix, int iy) {
    size_t focusTarget = items.size();
    for (size_t index = 0; index < items.size(); ++index) {
        const auto& item = items[index];
        if (item->focusable() &&
            item->rx <= ix && ix < item->rx + item->w &&
            item->ry <= iy && iy < item->ry + item->h
        ) {
            focusTarget = index;
            break;
        }
    }
    if (focusTarget == items.size()) clearFocus();
    else if (!hasFocused || focused != focusTarget) {
        clearFocus();
        focused = focusTarget;
        items[focused]->focus(true);
        hasFocused = true;
    }

    for (auto& i : items) {
        if ((i->rx <= ix && ix < i->rx + i->w) && (i->ry <= iy && iy < i->ry + i->h)) {
            i->onClick(ix - i->rx, iy - i->ry);
        }
        else i->onWideClick();
    }
}

void Layout::onWideClick() {
    clearFocus();
    for (auto& i : items) i->onWideClick();
}

void Layout::setAbs(int x, int y) {
    ax = x + rx;
    ay = y + ry;
    for (auto& i : items) i->setAbs(ax, ay);
}

Layout::Layout(int rx, int ry, int w, int h, FrameDrawer fd) : Widget(rx, ry, w, h), fd(fd) {}
