#pragma once

#include <vector>
#include "paintbrush/paintbrush.hpp"

class Widget {
public:
    int ax, ay, rx, ry, w, h;
    bool keyboardFocus = false;
    bool focusHighlight = true;

public:
    virtual ~Widget() = default;
    virtual void render(PaintBrush* pb, bool redraw) = 0;
    bool isFocus() const;
    bool isFocusHighlighted() const;
    bool isHover() const;
    void showFocus(bool visible);
    virtual bool focusable() const;
    virtual void focus(bool active);
    virtual bool onInput(int ch);
    virtual void onClick(int ix, int iy);
    virtual void onWideClick();
    virtual void setAbs(int x, int y);

public:
    Widget(int rx, int ry, int w, int h);
};
