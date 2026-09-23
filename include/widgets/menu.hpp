#pragma once

#include "widgets/widget.hpp"

#include <vector>
#include <map>
#include <utility>
#include <functional>

class Menu : public Widget {
private:
    int num_entries = 0, prev_focus = -1;
    bool changed = true;
    std::vector<std::pair<std::string, std::function<void()>>> entries;
    std::map<int, int> keymap;

    int getSelection();

public:
    void render(PaintBrush* pb, bool redraw) override;
    bool onInput(int ch) override;
    void onClick(int ix, int iy) override;
    void set(int ix, std::string text);

public:
    Menu(int x, int y, int w, std::vector<std::tuple<std::string, std::function<void()>, int>> entries);
};
