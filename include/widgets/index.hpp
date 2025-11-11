#pragma once

#include "widgets/widget.hpp"
#include <vector>
#include <memory>

class Index : public Widget {
private:
	std::vector<std::shared_ptr<Widget>> indexes;
	bool needRedraw = false;
public:
	void push(std::shared_ptr<Widget> w);
	void pop();
	void render(PaintBrush* pb, bool redraw) override;
	void onInput(int ch) override;
	void onClick(int ix, int iy) override;
    void onWideClick() override;
	void setabs(int x, int y) override;
	Index(int rx, int ry, int w, int h);
};