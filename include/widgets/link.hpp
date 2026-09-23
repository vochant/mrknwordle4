#pragma once

#include "widgets/widget.hpp"
#include <functional>

extern void empty_callback();

class Link : public Widget {
private:
    Color sf, sb, af, ab;
    int key;
    std::string content;
    std::function<void()> callback;
    bool needRedraw = true, prevIsFocus, prevHover;
    int renderedWidth = 0;

public:
    void render(PaintBrush* pb, bool redraw) override;
    void scolor(Color foreground);
    void scolor(Color foreground, Color background);
    void acolor(Color foreground);
    void acolor(Color foreground, Color background);
    std::string get();
    void set(std::string str, int len);
    bool onInput(int k) override;
    bool focusable() const override;
    void onClick(int ix, int iy) override;
    Link(
        int rx, int ry, int w, std::string str = "", int key = 0, std::function<void()> callback = empty_callback,
        Color sf = 15, Color sb = 0, Color af = 14, Color ab = 0
    );
};
