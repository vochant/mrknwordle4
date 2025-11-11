#include "widgets/datetime.hpp"

#include <ctime>
#include <cstdio>
#include "options.hpp"

void DateTime::render(PaintBrush* pb, bool redraw) {
	time_t now = time(nullptr);
	if (!redraw && now == last_check) return;
	last_check = now;

    UDate unow = static_cast<UDate>(now) * 1000.0;
    
    icu::UnicodeString result;
    fmt->format(unow, result);

    result.findAndReplace(u"\u202F", u" ");
    
    std::string utf8_result;
    result.toUTF8String(utf8_result);
    
	pb->locate(ax, ay);
	pb->color(f, b);
	pb->text(utf8_result);
}

void DateTime::color(short _f, short _b) {
	if (~_f) f = _f;
	if (~_b) b = _b;
}

DateTime::DateTime(int rx, int ry, short _f, short _b) : Widget(rx, ry, 50, 1), f(_f), b(_b), last_check(0) {
    fmt = icu::DateFormat::createDateTimeInstance(
        icu::DateFormat::FULL,
        icu::DateFormat::MEDIUM,
        icu::Locale((options->language + (options->calendar != "default" ? "@calendar=" + options->calendar : "")).c_str())
    );
}

DateTime::~DateTime() {
    delete fmt;
}
