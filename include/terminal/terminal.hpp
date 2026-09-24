#pragma once

#include "color.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Key {
    constexpr int Escape = 27;
    constexpr int Up = 0x110000, Down = 0x110001, Left = 0x110002, Right = 0x110003;
    constexpr int Delete = 0x110004, Home = 0x110005, End = 0x110006;
    constexpr int PageUp = 0x110007, PageDown = 0x110008;
    constexpr int CtrlLeft = 0x110009, CtrlRight = 0x11000a;
} // namespace Key

struct TermEvent {
    enum class Type { None, Key, Mouse, Resize, Close } type = Type::None;
    int key = 0, x = -1, y = -1;
    bool click = false, right = false;
};

struct TermCell {
    std::string text = " ";
    int foreground = 7, background = 0;
    int width = 1;
    uint16_t styles = 0;
    bool operator==(const TermCell& other) const;
};

struct TerminalGlyph {
    std::string text;
    int width = 1;
    bool replaced = false;
};

std::vector<std::string> split_graphemes(const std::string& text);
std::string encode_utf8(char32_t point);
TerminalGlyph prepare_glyph(const std::string& cluster, int ambiguousWidth = 1, bool singleBmp = false);

class Terminal {
    mutable std::unordered_map<std::string, int> glyphWidths;

protected:
    virtual int measureGlyphWidth(const std::string& glyph) const {
        return prepare_glyph(glyph, 1, singleBmpCells()).width;
    }

public:
    virtual ~Terminal() = default;
    virtual std::pair<int, int> size() = 0;
    virtual TermEvent poll(int timeoutMs) = 0;
    virtual void draw(int x, int y, const TermCell& cell) = 0;
    virtual void present() = 0;
    virtual void cursor(bool visible) = 0;
    virtual void setBackground(int) {}
    virtual void setTitle(const std::string&) {}
    virtual bool singleBmpCells() const { return false; }
    TerminalGlyph prepareGlyph(const std::string& cluster) const;
};

class TermScreen {
    int columns = 0, rows = 0;
    int background = 0;
    std::vector<TermCell> cells, prev;
    bool invalid = true;
    bool dirty = true;
    TermCell blank() const { return {" ", 7, background, 1, 0}; }
    void eraseGlyph(int x, int y);

public:
    void setBackground(int color);
    void resize(int width, int height);
    void put(int x, int y, TermCell cell);
    TermCell cell(int x, int y) const;
    void invalidate();
    void present(Terminal& terminal);
    int prevColumn(int x, int y) const;
};

extern std::unique_ptr<Terminal> term;
extern TermScreen termScreen;
struct TermOptions;
std::unique_ptr<Terminal> create_term(const TermOptions& options);
std::unique_ptr<Terminal> create_curses_term(const TermOptions& options);
std::unique_ptr<Terminal> create_ansi_term(bool mouse);
std::unique_ptr<Terminal> create_win32_term(bool mouse);
std::unique_ptr<Terminal> create_win32_vt_term(bool mouse);
bool ansi_term_available();
bool term_interrupted();
void install_term_signals();
void restore_term_signals();
