#pragma once

#include <string>
#include <vector>
#include <set>
#include "char.hpp"
#include "terminal/terminal.hpp"

class PaintBrush {
    int x = 0, y = 0;
    int foreground = 7, background = 0;
    uint16_t styles = 0, foregroundStyles = 0, backgroundStyles = 0;
    bool foregroundHidden = false, backgroundHidden = false;
    std::string pending;
    bool hidden() const { return foregroundHidden || backgroundHidden; }
    void blank(int x, int y, int width);
    void glyph(const std::string& cluster);
    TerminalGlyph prepareSymbol(Char symbol) const;
    void placeSymbol(int x, int y, const TerminalGlyph& glyph);
    void horiSymbols(int start, int end, int y, Char symbol);

public:
    short f, b;
    int align_x;

public:
    void single(const char ch);
    void color(const short _f = -1, const short _b = -1);
    void color(Color foreground, Color background = Color(0));
    void forergb(const short r, const short g, const short b);
    void backrgb(const short r, const short g, const short b);
    void line_forward();
    void locate(const int _x, const int _y);
    void back();

public:
    void symbol(Char sym);
    void text(const char* const str);
    void text(const std::string& str);
    int textBox(const std::string& text, int width, int height, bool wrap = true, int firstLine = 0);
    void rect(const int sx, const int sy, const int ex, const int ey, const bool doFill);
    void fill(const int sx, const int sy, const int ex, const int ey);
    void vline(const int _x, const int sy, const int ey, const std::set<int> cross = {});
    void hline(const int sx, const int ex, const int _y, const std::set<int> cross = {});
    void setf(const short _f);
    void setb(const short _b);

public:
    PaintBrush();
};
