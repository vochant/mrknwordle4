#pragma once

#include "widgets/layout.hpp"
#include <functional>

void emptyCallback();

class Button : public Widget {
private:
	std::function<void()> callback;
	std::shared_ptr<Widget> inner;
	bool prevIsFocus;
    int key;
public:
	void render(PaintBrush* pb, bool redraw) override;
	void onInput(int k) override;
	void onClick(int ix, int iy) override;
	void set(std::shared_ptr<Widget> w);
	void setabs(int x, int y) override;
public:
	Button(int rx, int ry, int w, int h, int key = 0, std::function<void()> callback = emptyCallback);
};