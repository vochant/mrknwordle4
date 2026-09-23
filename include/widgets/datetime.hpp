#pragma once
#include "widgets/widget.hpp"
#include <ctime>
class DateTime : public Widget {
    time_t last_check = -1;
    Color foreground, background;

public:
    void render(PaintBrush* brush, bool redraw) override;
    void color(Color foreground);
    void color(Color foreground, Color background);
    DateTime(int x, int y, Color foreground = 15, Color background = 0);
};
