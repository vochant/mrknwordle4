#pragma once

#include "widgets/widget.hpp"
#include <functional>
#include "render.hpp"

void emptyCallbackWithBoolean(bool v);

class Switch : public Widget {
private:
	bool status, needRedraw, prevIsFocus;
	std::function<void(bool)> callback;
public:
	void render(PaintBrush* pb, bool redraw) override;
	bool get();
	void onInput(int k) override;
	void onClick(int ix, int iy) override;
	Switch(int rx, int ry, std::function<void(bool)> callback = emptyCallbackWithBoolean, bool Init = false);
};