#pragma once

#include "widgets/widget.hpp"
#include <functional>
#include <string>

void empty_bool_callback(bool v);

class Switch : public Widget {
private:
    bool status, needRedraw, prevIsFocus, prevHover;
    int key;
    std::function<void(bool)> callback;
    std::string offText, onText;

public:
    void render(PaintBrush* pb, bool redraw) override;
    bool get() const;
    void set(bool value);
    bool onInput(int k) override;
    bool focusable() const override;
    void onClick(int ix, int iy) override;
    Switch(
        int rx, int ry, int w, std::string offText, std::string onText, int key = 0,
        std::function<void(bool)> callback = empty_bool_callback, bool Init = false
    );
};
