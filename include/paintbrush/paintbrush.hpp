#pragma once

#include "config.hpp"
#include <string>
#include <vector>
#include <set>
#include "char.hpp"
#include <windows.h>

class PaintBrush {
public:
	short f, b;
    int align_x;
public:
	void single(const char ch);
	void color(const short _f = -1, const short _b = -1);
    void forergb(const short r, const short g, const short b);
    void backrgb(const short r, const short g, const short b);
	void line_forward();
	void locate(const int _x, const int _y);
	void back();
public:
	void symbol(Char sym);
	void text(const char* const str);
	void text(const std::string str);
	void rect(const int sx, const int sy, const int ex, const int ey, const bool doFill);
	void fill(const int sx, const int sy, const int ex, const int ey);
	void vline(const int _x, const int sy, const int ey, const std::set<int> cross = {});
	void hline(const int sx, const int ex, const int _y, const std::set<int> cross = {});
	void setf(const short _f);
	void setb(const short _b);
public:
	PaintBrush();
};

extern PaintBrush* pb;