#pragma once

#include "widgets/layout.hpp"
#include <functional>

void empty_callback();

class Button : public Widget {
private:
    std::function<void()> callback;
    std::unique_ptr<Widget> inner;
    bool prevIsFocus, prevHover;
    int key;

public:
    void render(PaintBrush* pb, bool redraw) override;
    bool onInput(int k) override;
    bool focusable() const override;
    void onClick(int ix, int iy) override;
    void set(std::unique_ptr<Widget> w);
    void setAbs(int x, int y) override;

public:
    Button(int rx, int ry, int w, int h, int key = 0, std::function<void()> callback = empty_callback);
};
