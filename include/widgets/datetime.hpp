#pragma once

#include "widgets/widget.hpp"
#include <unicode/datefmt.h>
#include <unicode/unistr.h>
#include <unicode/calendar.h>

class DateTime : public Widget {
private:
	time_t last_check;
	short f, b;
    icu::DateFormat* fmt;

public:
	void render(PaintBrush* pb, bool redraw) override;
	void color(short _f = -1, short _b = -1);
	DateTime(int rx, int ry, short _f = 15, short _b = 0);
    ~DateTime();
};