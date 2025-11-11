#pragma once

#include "widgets/widget.hpp"
#include <vector>
#include <memory>
#include <functional>

void no_frame(int x, int y, int w, int h, PaintBrush* pb);
void single_frame(int x, int y, int w, int h, PaintBrush* pb);

class Layout : public Widget {
public:
	typedef std::function<void(int, int, int, int, PaintBrush*)> FrameDrawer;
private:
	std::vector<std::shared_ptr<Widget>> items;
	FrameDrawer fd;
public:
	void render(PaintBrush* pb, bool redraw) override;
	void add(std::shared_ptr<Widget> w);
	void onInput(int ch) override;
	void onClick(int ix, int iy) override;
    void onWideClick() override;
	void setabs(int x, int y) override;
public:
	Layout(int rx, int ry, int w, int h, FrameDrawer fd);
};