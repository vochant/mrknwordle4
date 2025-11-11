#pragma once

#include "widgets/widget.hpp"
#include <functional>

extern void emptyCallback();

class Link : public Widget {
private:
	short sf, sb, af, ab;
    int key;
	std::string content;
	std::function<void()> callback;
	bool needRedraw, prevIsFocus;
public:
	void render(PaintBrush* pb, bool redraw) override;
	void scolor(short _f = -1, short _b = -1);
	void acolor(short _f = -1, short _b = -1);
	std::string get();
	void set(std::string str, int len);
	void onInput(int k) override;
	void onClick(int ix, int iy) override;
	Link(int rx, int ry, int w, std::string str = "", int key = 0, std::function<void()> callback = emptyCallback, int sf = 15, int sb = 0, int af = 14, int ab = 0);
};