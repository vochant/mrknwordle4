#pragma once

#include "widgets/widget.hpp"

struct IconSymbol {
	int rx, ry;
	short f, b;
	std::string sym;
};

#include <vector>

class Icon : public Widget {
private:
	std::vector<IconSymbol> bits;
public:
	void render(PaintBrush* pb, bool redraw) override;
	Icon(int rx, int ry, int w, int h, std::vector<IconSymbol> bits);
};