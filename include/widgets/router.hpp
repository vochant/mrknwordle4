#pragma once

#include "widgets/widget.hpp"
#include <map>
#include <memory>

class Router : public Widget {
public:
	std::map<std::string, std::shared_ptr<Widget>> p;
	std::shared_ptr<Widget> current;
	bool needRedraw;
	std::string current_name;
public:
	void add(std::string name, std::shared_ptr<Widget> e);
	void route(std::string name);
	void render(PaintBrush* pb, bool redraw) override;
	void onInput(int ch) override;
	void onClick(int ix, int iy) override;
    void onWideClick() override;
	void setabs(int x, int y) override;
public:
	Router(int rx, int ry, int w, int h);
};