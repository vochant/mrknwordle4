#include "widgets/menu.hpp"
#include "options.hpp"
#include "services/mouse.service.hpp"
#include "render.hpp"

int Menu::getSelection() {
    if (!options->mouseControlling) return -1;
    int my = mouse_service->where().second;
    if (my < ay || my >= ay + h || my < ax || my >= ax + w) return -1;
    return my - ay;
}

void Menu::render(PaintBrush* pb, bool redraw) {
    int sel = getSelection();
    if (sel != prev_focus) prev_focus = sel, redraw = true;
    if (!redraw) return;
    for (int i = 0; i < num_entries; i++) {
        pb->locate(ax, ay + i);
        pb->color(i == sel ? 14 : 15, 0);
        pb->text(entries[i].first + std::string(w - entries[i].first.length(), ' '));
    }
}

void Menu::onClick(int ix, int iy) {
    if (iy < 0 || iy >= num_entries) return;
    entries[iy].second();
}

void Menu::onInput(int ch) {
    if (prev_focus != -1 && ch == '\r') {
        entries[prev_focus].second();
    }
    else if (keymap.count(ch)) {
        entries[keymap[ch]].second();
    }
}

void Menu::set(int ix, std::string text) {
    if (ix < 0 || ix >= num_entries) return;
    renderer_lock.lock();
    entries[ix].first = text;
    renderer_lock.unlock();
}

Menu::Menu(int x, int y, int w, std::vector<std::tuple<std::string, std::function<void()>, int>> entries) : Widget(x, y, w, entries.size()) {
    num_entries = 0;
    for (const auto& entry : entries) {
        this->entries.emplace_back(std::get<0>(entry), std::get<1>(entry));
        if (std::get<2>(entry)) keymap[std::get<2>(entry)] = num_entries;
        num_entries++;
    }
}