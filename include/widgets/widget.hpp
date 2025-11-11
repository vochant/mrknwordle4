#pragma once

#include <vector>
#include "paintbrush/paintbrush.hpp"

class Widget {
public:
	int ax, ay, rx, ry, w, h;
public:
	virtual void render(PaintBrush* pb, bool redraw) = 0;
	bool isFocus();
	virtual void onInput(int ch);
	virtual void onClick(int ix, int iy);
    virtual void onWideClick();
	virtual void setabs(int x, int y);
public:
	Widget(int rx, int ry, int w, int h);
};