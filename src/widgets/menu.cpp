#include "widgets/menu.hpp"
#include "options.hpp"
#include "services/mouse.service.hpp"

int Menu::getSelection() {
    if (!options->term.mouse) return -1;
    auto pos = mouse_service->where();
    int my = pos.second;
    if (my < ay || my >= ay + h || pos.first < ax || pos.first >= ax + w) return -1;
    return my - ay;
}

void Menu::render(PaintBrush* pb, bool redraw) {
    redraw |= changed;
    changed = false;
    int sel = getSelection();
    if (sel != prev_focus) prev_focus = sel, redraw = true;
    if (!redraw) return;
    for (int i = 0; i < num_entries; i++) {
        pb->locate(ax, ay + i);
        pb->color(i == sel ? options->colors.hover : options->colors.foreground, options->colors.background);
        pb->fill(ax, ay + i, ax + w - 1, ay + i);
        pb->locate(ax, ay + i);
        pb->textBox(entries[i].first, w, 1, false);
    }
}

void Menu::onClick(int, int iy) {
    if (iy < 0 || iy >= num_entries) return;
    entries[iy].second();
}

bool Menu::onInput(int ch) {
    if (prev_focus != -1 && ch == '\r') {
        entries[prev_focus].second();
        return true;
    }
    if (keymap.count(ch)) {
        entries[keymap[ch]].second();
        return true;
    }
    return false;
}

void Menu::set(int ix, std::string text) {
    if (ix < 0 || ix >= num_entries) return;

    entries[ix].first = text;
    changed = true;
}

Menu::Menu(
    int x, int y, int w,
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries
) : Widget(x, y, w, entries.size()) {
    num_entries = 0;
    for (const auto& entry : entries) {
        this->entries.emplace_back(std::get<0>(entry), std::get<1>(entry));
        if (std::get<2>(entry)) keymap[std::get<2>(entry)] = num_entries;
        num_entries++;
    }
}
