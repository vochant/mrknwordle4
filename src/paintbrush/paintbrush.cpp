#include "paintbrush/paintbrush.hpp"
#include "global.h"
#include "options.hpp"

#include <cstdio>

void PaintBrush::text(const char* const str) {
	const char* ptr = str;
	while (*ptr) {
        if (*ptr == '\n') line_forward();
        else if (*ptr == 1) {
            ptr++;
            color(*ptr & 15, *ptr >> 4);
        }
		else single(*ptr);
		ptr++;
	}
}

void PaintBrush::text(const std::string str) {
	text(str.c_str());
}

void PaintBrush::symbol(const Char sym) {
	text(Lookup(sym));
}

void PaintBrush::rect(const int sx, const int sy, const int ex, const int ey, const bool doFill) {
	locate(sx, sy);
	int w = ex - sx - 1;
	symbol(Char::DR);
	for (int i = 0; i < w; i++) symbol(Char::LR);
	symbol(Char::DL);
	for (int y = sy + 1; y < ey; y++) {
		locate(sx, y);
		symbol(Char::UD);
		locate(ex, y);
		symbol(Char::UD);
	}
	locate(sx, ey);
	symbol(Char::UR);
	for (int i = 0; i < w; i++) symbol(Char::LR);
	symbol(Char::UL);
	if (doFill) {
		fill(sx + 1, sy + 1, ex - 1, ey - 1);
	}
}

void PaintBrush::fill(const int sx, const int sy, const int ex, const int ey) {
	for (int i = sy; i <= ey; i++) {
		locate(sx, i);
		for (int i = sx; i <= ex; i++) single(' ');
	}
}

void PaintBrush::hline(const int sx, const int ex, const int _y, const std::set<int> cross) {
	auto it = cross.begin();
	locate(sx, _y);
	for (int i = sx; i <= ex; i++) {
		if (it != cross.end() && *it == i) {
			if (i == sx) symbol(Char::UDR);
			else if (i == ex) symbol(Char::UDL);
			else symbol(Char::UDLR);
			it++;
		}
		else symbol(Char::LR);
	}
}

void PaintBrush::vline(const int _x, const int sy, const int ey, const std::set<int> cross) {
	auto it = cross.begin();
	for (int i = sy; i <= ey; i++) {
		locate(_x, i);
		if (it != cross.end() && *it == i) {
			if (i == sy) symbol(Char::DLR);
			else if (i == ey) symbol(Char::ULR);
			else symbol(Char::UDLR);
			it++;
		}
		else symbol(Char::UD);
	}
}

void PaintBrush::setf(const short _f) {
	f = _f;
	color();
}

void PaintBrush::setb(const short _b) {
	b = _b;
	color();
}

void PaintBrush::locate(const int _x, const int _y) {
    if (options->virtualTerminal) {
        printf("\x1b[%d;%dH", _y + 1, _x + 1);
        align_x = _x;
    }
    else {
        COORD coord = {(short)_x, (short)_y};
        align_x = _x;
        SetConsoleCursorPosition(hOutput, coord);
    }
}

void PaintBrush::single(const char ch) {
	putchar(ch);
}

void PaintBrush::line_forward() {
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hOutput, &csbi);
    locate(align_x, csbi.dwCursorPosition.Y + 1);
}

void PaintBrush::forergb(const short r, const short g, const short b) {
    printf("\x1b[38;2;%d;%d;%dm", (int) r, (int) g, (int) b);
}

void PaintBrush::backrgb(const short r, const short g, const short b) {
    printf("\x1b[48;2;%d;%d;%dm", (int) r, (int) g, (int) b);
}

void PaintBrush::color(const short _f, const short _b) {
	if (~_f) f = _f;
	if (~_b) b = _b;
    if (options->virtualTerminal) {
        printf("\x1b[0;%d%d;%d%dm", (f & 8) ? 9 : 3, (f & 4) >> 2 | (f & 2) | (f & 1) << 2, (b & 8) ? 10 : 4, (b & 4) >> 2 | (b & 2) | (b & 1) << 2);
    }
    else {
        SetConsoleTextAttribute(hOutput, (b << 4) | f);
    }
}

void PaintBrush::back() {
	fputs("\b \b", stdout);
}

PaintBrush::PaintBrush() : f(7), b(0), align_x(0) {}