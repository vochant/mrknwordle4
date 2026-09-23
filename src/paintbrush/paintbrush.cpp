#include "paintbrush/paintbrush.hpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <algorithm>

namespace {
    void require_even(int coord) {
        if (coord % 2) {
            throw std::logic_error("Frame coordinates must use even terminal columns");
        }
    }
} // namespace

void PaintBrush::text(const char* const str) {
    const char* ptr = str;
    while (*ptr) {
        if (*ptr == '\n') { line_forward(); }
        else if (*ptr == 2) {
            const char* end = std::strchr(ptr + 1, 3);
            if (!end) break;
            std::string expr(ptr + 1, end);
            auto sep = expr.find(';');
            try {
                if (sep != std::string::npos) {
                    color(
                        parse_color(expr.substr(0, sep)),
                        parse_color(expr.substr(sep + 1))
                    );
                }
            }
            catch (const std::invalid_argument&) {}
            ptr = end;
        }
        else if ((unsigned char) *ptr >= 32) {
            const char* begin = ptr;
            while (*ptr && (unsigned char) *ptr >= 32) ptr++;
            std::string run(begin, ptr);
            if (std::all_of(run.begin(), run.end(), [](unsigned char byte) { return byte < 127; })) {
                for (char byte : run) single(byte);
            }
            else {
                pending.clear();
                for (const auto& cluster : split_graphemes(run)) glyph(cluster);
            }
            continue;
        }
        ptr++;
    }
}

void PaintBrush::text(const std::string& str) { text(str.c_str()); }

void PaintBrush::symbol(const Char sym) {
    auto prepared = prepareSymbol(sym);
    placeSymbol(x, y, prepared);
    x += prepared.width;
}

TerminalGlyph PaintBrush::prepareSymbol(Char symbol) const {
    auto text = Lookup(symbol);
    auto prepared = term ? term->prepareGlyph(text) : prepare_glyph(text);
    if (!prepared.replaced) return prepared;
    if (symbol == Char::UD) return {"|", 1, false};
    if (symbol == Char::LR) return {"-", 1, false};
    return {"+", 1, false};
}

void PaintBrush::placeSymbol(int column, int row, const TerminalGlyph& glyph) {
    if (hidden()) {
        blank(column, row, glyph.width);
        return;
    }
    termScreen.put(column, row, {glyph.text, foreground, background, glyph.width, styles});
}

void PaintBrush::blank(int column, int row, int width) {
    for (int offset = 0; offset < width; offset++) {
        termScreen.put(column + offset, row, {" ", foreground, background, 1, styles});
    }
}

void PaintBrush::horiSymbols(int start, int end, int row, Char symbol) {
    auto prepared = prepareSymbol(symbol);
    for (int column = start; column + prepared.width <= end; column += prepared.width) {
        placeSymbol(column, row, prepared);
    }
}

void PaintBrush::rect(const int sx, const int sy, const int ex, const int ey, const bool doFill) {
    require_even(sx);
    require_even(ex);
    if (sx >= ex || sy > ey) return;
    auto tl = prepareSymbol(Char::DR);
    auto tr = prepareSymbol(Char::DL);
    auto bl = prepareSymbol(Char::UR);
    auto br = prepareSymbol(Char::UL);
    auto cv = prepareSymbol(Char::UD);
    int trx = ex - tr.width;
    int brx = ex - br.width;
    int rx = ex - cv.width;

    placeSymbol(sx, sy, tl);
    horiSymbols(sx + tl.width, trx, sy, Char::LR);
    placeSymbol(trx, sy, tr);
    for (int y = sy + 1; y < ey; y++) {
        placeSymbol(sx, y, cv);
        placeSymbol(rx, y, cv);
    }
    placeSymbol(sx, ey, bl);
    horiSymbols(sx + bl.width, brx, ey, Char::LR);
    placeSymbol(brx, ey, br);
    if (doFill) {
        int leftInset = std::max({tl.width, bl.width, cv.width});
        int rightInset = std::max({tr.width, br.width, cv.width});
        fill(sx + leftInset, sy + 1, ex - rightInset - 1, ey - 1);
    }
}

void PaintBrush::fill(const int sx, const int sy, const int ex, const int ey) {
    for (int i = sy; i <= ey; i++) {
        locate(sx, i);
        for (int i = sx; i <= ex; i++) single(' ');
    }
}

void PaintBrush::hline(const int sx, const int ex, const int _y, const std::set<int> cross) {
    require_even(sx);
    require_even(ex);
    if (sx >= ex) return;
    auto ch = prepareSymbol(Char::LR);
    int column = sx;
    for (int crossing : cross) {
        require_even(crossing);
        if (crossing < sx || crossing > ex) continue;
        Char type = crossing == sx ? Char::UDR : crossing == ex ? Char::UDL : Char::UDLR;
        auto junction = prepareSymbol(type);
        int jx = crossing == ex ? ex - junction.width : crossing;
        while (column + ch.width <= jx) {
            placeSymbol(column, _y, ch);
            column += ch.width;
        }
        if (column < jx) column = jx;
        placeSymbol(jx, _y, junction);
        column = std::max(column, jx + junction.width);
    }
    while (column + ch.width <= ex) {
        placeSymbol(column, _y, ch);
        column += ch.width;
    }
}

void PaintBrush::vline(const int _x, const int sy, const int ey, const std::set<int> cross) {
    require_even(_x);
    if (sy > ey) return;
    for (int i = sy; i <= ey; i++) {
        Char type = cross.count(i) ? (i == sy ? Char::DLR : i == ey ? Char::ULR : Char::UDLR) : Char::UD;
        placeSymbol(_x, i, prepareSymbol(type));
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
    pending.clear();
    x = align_x = _x;
    y = _y;
}

void PaintBrush::single(const char ch) {
    auto byte = (unsigned char) ch;
    if (pending.empty() && byte < 32) return;
    if (pending.empty() && byte < 127) {
        termScreen.put(x++, y, {hidden() ? " " : std::string(1, ch), foreground, background, 1, styles});
        return;
    }
    if (!pending.empty() && (byte & 0xc0) != 0x80) pending.clear();
    pending += ch;
    auto first = (unsigned char) pending.front();
    size_t length = first < 0x80 ? 1 :
        first >= 0xc2 && first <= 0xdf ? 2 :
        first >= 0xe0 && first <= 0xef ? 3 :
        first >= 0xf0 && first <= 0xf4 ? 4 : 0;
    if (!length) {
        pending.clear();
        return;
    }
    if (pending.size() < length) return;
    char32_t codepoint = first & (length == 1 ? 0x7f : length == 2 ? 0x1f : length == 3 ? 0x0f : 0x07);
    for (size_t index = 1; index < length; ++index) codepoint = (codepoint << 6) | (pending[index] & 0x3f);
    if ((length == 2 && codepoint < 0x80) ||
        (length == 3 && codepoint < 0x800) ||
        (length == 4 && codepoint < 0x10000) ||
        codepoint > 0x10ffff ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff)
    ) {
        pending.clear();
        return;
    }
    glyph(pending);
    pending.clear();
}

void PaintBrush::glyph(const std::string& cluster) {
    auto prepared = term ? term->prepareGlyph(cluster) : prepare_glyph(cluster);
    if (hidden()) blank(x, y, prepared.width);
    else termScreen.put(x, y, {prepared.text, foreground, background, prepared.width, styles});
    x += prepared.width;
}

void PaintBrush::line_forward() { locate(align_x, y + 1); }

void PaintBrush::forergb(const short r, const short g, const short b) {
    foreground = 0x1000000 | ((r & 255) << 16) | ((g & 255) << 8) | (b & 255);
    foregroundHidden = false;
}

void PaintBrush::backrgb(const short r, const short g, const short b) {
    background = 0x1000000 | ((r & 255) << 16) | ((g & 255) << 8) | (b & 255);
    backgroundHidden = false;
}

void PaintBrush::color(const short _f, const short _b) {
    if (~_f) f = _f;
    if (~_b) b = _b;
    styles = foregroundStyles = backgroundStyles = 0;
    foregroundHidden = backgroundHidden = false;
    foreground = f & 15;
    background = b & 15;
}

void PaintBrush::back() {
    if (x > 0) {
        x = termScreen.prevColumn(x, y);
        termScreen.put(x, y, {" ", foreground, background, 1, styles});
    }
}

PaintBrush::PaintBrush() : f(7), b(0), align_x(0) {}

void PaintBrush::color(Color fore, Color back) {
    foregroundHidden = fore.isHidden();
    backgroundHidden = back.isHidden();
    foregroundStyles = fore.isHidden() ? 0 : fore.styles;
    backgroundStyles = back.isHidden() ? 0 : back.styles;
    if (!foregroundHidden) {
        foreground = fore.value;
        f = nearest_term_color(fore.value);
    }
    if (!backgroundHidden) {
        background = back.value;
        b = nearest_term_color(back.value);
    }
    styles = hidden() ? 0 : foregroundStyles | backgroundStyles;
}

int PaintBrush::textBox(const std::string& value, int width, int height, bool wrap, int firstLine) {
    if (width <= 0 || height <= 0) return 0;
    const int originX = x, originY = y;
    int column = 0, row = -std::max(0, firstLine);
    bool clipped = false;
    size_t offset = 0;
    while (offset < value.size()) {
        if (value[offset] == '\x02') {
            auto end = value.find('\x03', offset + 1);
            if (end == std::string::npos) break;
            text(value.substr(offset, end - offset + 1));
            offset = end + 1;
            continue;
        }
        if (value[offset] == '\n') {
            row++;
            column = 0;
            clipped = false;
            offset++;
            if (row >= 0 && row < height) locate(originX, originY + row);
            continue;
        }
        auto end = value.find_first_of("\x02\n", offset);
        if (end == std::string::npos) end = value.size();
        for (const auto& cluster : split_graphemes(value.substr(offset, end - offset))) {
            auto prepared = term ? term->prepareGlyph(cluster) : prepare_glyph(cluster);
            if (column + prepared.width > width) {
                if (!wrap) {
                    clipped = true;
                    break;
                }
                row++;
                column = 0;
                if (row >= 0 && row < height) locate(originX, originY + row);
            }
            if (!clipped && prepared.width <= width) {
                if (row >= 0 && row < height) text(prepared.text);
                column += prepared.width;
            }
        }
        offset = end;
    }
    return row + std::max(0, firstLine) + 1;
}
