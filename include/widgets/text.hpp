#pragma once

#include "widgets/widget.hpp"

class Text : public Widget {
private:
	std::string text;
	short f, b;
	bool needRedraw;
public:
	std::string get();
	void set(std::string str, int len);
	virtual void render(PaintBrush* pb, bool redraw) override;
	void color(short _f = -1, short _b = -1);
	Text(int rx, int ry, int w, std::string str = "", int f = 15, int b = 0);
};