#include "widgets/icon.hpp"

void Icon::render(PaintBrush* pb, bool redraw) {
	if (!redraw) return;
	for (auto& i : bits) {
		pb->locate(ax + i.rx, ay + i.ry);
		pb->color(i.f, i.b);
		pb->text(i.sym);
	}
}

Icon::Icon(int rx, int ry, int w, int h, std::vector<IconSymbol> bits) : Widget(rx, ry, w, h), bits(bits) {}