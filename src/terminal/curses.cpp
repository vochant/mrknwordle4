#include "terminal/terminal.hpp"
#include "options.hpp"

#include <clocale>
#include <cstdio>
#include <map>
#include <stdexcept>
#include <climits>
#include <cwchar>
#include <limits>
#include <unicode/unistr.h>
#include <curses.h>
#ifdef NCURSES_VERSION
#include <term.h>
#endif
#if !defined(_WIN32) && !defined(WORDLE_CURSES_PDCURSES)
#include <unistd.h>
#include <poll.h>
#endif

namespace {
#ifdef NCURSES_VERSION
    constexpr int cursesCtrlLeft = KEY_MAX + 1;
    constexpr int cursesCtrlRight = KEY_MAX + 2;

    void defineTerminfoKey(const char* capability, int key) {
        char* seq = tigetstr(const_cast<char*>(capability));
        if (seq && seq != reinterpret_cast<char*>(-1)) define_key(seq, key);
    }
#endif
    class CursesTerminal final : public Terminal {
        enum class ColorModel { Direct, PdcPalette, Xterm256, Xterm88, Ansi16, Ansi8 };
        SCREEN* screen = nullptr;
        WINDOW* widthProbe = nullptr;
        int oldCursor = 1;
        std::map<std::pair<int, int>, int> pairs;
        std::map<int, int> pdcPalette;
        int nextPair = 1;
        int nextPdcColor = 256;
        const TermOptions& options;
        static int terminalColorIndex(int color) {
            static const int mapping[] = {0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15};
            return mapping[color & 15];
        }
        static int ansiRgb(int index) {
            static const int ansi[] = {
                0x000000, 0x800000, 0x008000, 0x808000, 0x000080, 0x800080, 0x008080, 0xc0c0c0,
                0x808080, 0xff0000, 0x00ff00, 0xffff00, 0x0000ff, 0xff00ff, 0x00ffff, 0xffffff
            };
            return ansi[index & 15];
        }
        static bool termColor(int color) { return color >= 0 && color <= 15; }
        static int rgb(int color) { return isRgbColor(color) ? rgbValue(color) : ansiRgb(terminalColorIndex(color)); }
        static int xtermComponent(int index) {
            static const int levels[] = {0, 95, 135, 175, 215, 255};
            return levels[index];
        }
        static int xtermRgb(int index, int colors) {
            if (index < 16) return ansiRgb(index);
            if (colors == 88) {
                static const int levels[] = {0, 139, 205, 255};
                if (index < 80) {
                    int cube = index - 16;
                    return (levels[cube / 16] << 16) | (levels[(cube / 4) % 4] << 8) | levels[cube % 4];
                }
                static const int gray[] = {46, 92, 115, 139, 162, 185, 208, 231};
                int level = gray[index - 80];
                return (level << 16) | (level << 8) | level;
            }
            if (index < 232) {
                int cube = index - 16;
                return (xtermComponent(cube / 36) << 16) | (xtermComponent((cube / 6) % 6) << 8) |
                    xtermComponent(cube % 6);
            }
            int level = 8 + (index - 232) * 10;
            return (level << 16) | (level << 8) | level;
        }
        static int nearestXtermColor(int color, int colors) {
            int best = 0, dist = std::numeric_limits<int>::max();
            for (int i = 0; i < colors; ++i) {
                int cand = 0;
                for (int shift : {0, 8, 16}) {
                    int diff = ((color >> shift) & 255) - ((xtermRgb(i, colors) >> shift) & 255);
                    cand += diff * diff;
                }
                if (cand < dist) {
                    dist = cand;
                    best = i;
                }
            }
            return best;
        }
        bool directColors() const {
#ifdef WORDLE_CURSES_PDCURSES
            return COLORS >= 0x1000100;
#else
            return COLORS >= 0x1000000;
#endif
        }
        bool keepAnsi16() const {
            return directColors() && options.cursesAnsi16 == CursesAnsi16::On && options.cursesReservedColors >= 16;
        }
        ColorModel colorModel() const {
            if (directColors()) return ColorModel::Direct;
#ifdef WORDLE_CURSES_PDCURSES
            if (COLORS >= 768) return ColorModel::PdcPalette;
#endif
            if (COLORS == 256) return ColorModel::Xterm256;
            if (COLORS == 88) return ColorModel::Xterm88;
            return COLORS >= 16 ? ColorModel::Ansi16 : ColorModel::Ansi8;
        }
        int palette(int color, bool native) {
            const auto model = colorModel();
            if (model == ColorModel::Direct) {
                if (native && termColor(color)) return terminalColorIndex(color);
                const int value = rgb(color);
#ifdef WORDLE_CURSES_PDCURSES
                return 256 + ((value & 0xff) << 16) + (value & 0xff00) + ((value >> 16) & 0xff);
#else
                return value < options.cursesReservedColors ? value + 256 : value;
#endif
            }
#ifdef WORDLE_CURSES_PDCURSES
            if (model == ColorModel::PdcPalette) {
                if (termColor(color)) return terminalColorIndex(color);
                const int value = rgb(color);
                const auto found = pdcPalette.find(value);
                if (found != pdcPalette.end()) return found->second;
                if (nextPdcColor < COLORS) {
                    const int slot = nextPdcColor++;
                    const int red = ((value >> 16) & 255) * 1000 / 255;
                    const int green = ((value >> 8) & 255) * 1000 / 255;
                    const int blue = (value & 255) * 1000 / 255;
                    if (init_extended_color(slot, red, green, blue) == OK) {
                        pdcPalette.emplace(value, slot);
                        return slot;
                    }
                }
                return nearestXtermColor(value, 256);
            }
#endif
            if (!termColor(color) && model == ColorModel::Xterm256) return nearestXtermColor(rgb(color), 256);
            if (!termColor(color) && model == ColorModel::Xterm88) return nearestXtermColor(rgb(color), 88);
            int index = terminalColorIndex(nearest_term_color(color));
            return model == ColorModel::Ansi8 ? index & 7 : index;
        }
        static std::wstring wide(const std::string& text) {
            const auto unicode = icu::UnicodeString::fromUTF8(text);
            std::wstring res;
            for (int32_t offset = 0; offset < unicode.length();) {
                auto cp = unicode.char32At(offset);
                offset += U16_LENGTH(cp);
                if (sizeof(wchar_t) == 2 && cp > 0xffff) return L"?";
                res += (wchar_t) cp;
            }
            return res;
        }
        int measuredWidth(const std::string& text) const {
            if (!widthProbe) return 0;
            werase(widthProbe);
            const auto value = wide(text);
            if (wmove(widthProbe, 0, 0) == ERR || waddwstr(widthProbe, value.c_str()) == ERR) return 0;
            int row, column;
            getyx(widthProbe, row, column);
            return row == 0 && (column == 1 || column == 2) ? column : 0;
        }

    public:
        bool singleBmpCells() const override { return sizeof(wchar_t) == 2; }
        TerminalGlyph prepareGlyph(const std::string& cluster) const override {
            auto glyph = Terminal::prepareGlyph(cluster);
            if (glyph.replaced) return glyph;
            int width = measuredWidth(glyph.text);
            if (width) glyph.width = width;
            else return {"?", 1, true};
            return glyph;
        }
        explicit CursesTerminal(const TermOptions& options) : options(options) {
#if !defined(_WIN32) && !defined(WORDLE_CURSES_PDCURSES)
            if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO)) {
                throw std::runtime_error("curses backend requires an interactive terminal");
            }
#endif
            const char* term = options.cursesTerm.empty() ? nullptr : options.cursesTerm.c_str();
#ifdef WORDLE_CURSES_PDCURSES
            // The UI is designed for the 80x25 minimum. PDCurses defaults to
            // 80x24, so request the application size before creating the screen.
            // Keep explicit PDC_LINES/PDC_COLS overrides untouched.
            if (!std::getenv("PDC_LINES") && !std::getenv("PDC_COLS")) resize_term(25, 80);
#endif
            screen = newterm(term, stdout, stdin);
            if (!screen) throw std::runtime_error("Cannot initialize curses terminal (check TERM and tty)");
            set_term(screen);
            cbreak();
            noecho();
            keypad(stdscr, TRUE);
            oldCursor = curs_set(0);
            if (!has_colors()) {
                endwin();
                delscreen(screen);
                screen = nullptr;
                throw std::runtime_error("curses backend requires at least 8 colors");
            }
            start_color();
            if (COLORS < 8) {
                endwin();
                delscreen(screen);
                screen = nullptr;
                throw std::runtime_error("curses backend requires at least 8 colors");
            }
            if (options.mouse) {
                mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, nullptr);
                mouseinterval(0);
            }
            widthProbe = newpad(1, 8);
            int ambiguousWidth = measuredWidth("│");
            setAmb(ambiguousWidth);
#ifdef NCURSES_VERSION
            set_escdelay(30);
            defineTerminfoKey("kLFT5", cursesCtrlLeft);
            defineTerminfoKey("kRIT5", cursesCtrlRight);
#endif
        }
        ~CursesTerminal() override {
            if (widthProbe) delwin(widthProbe);
            mousemask(0, nullptr);
            curs_set(oldCursor == ERR ? 1 : oldCursor);
            endwin();
            delscreen(screen);
        }
        std::pair<int, int> size() override {
            int height, width;
            getmaxyx(stdscr, height, width);
            return {width, height};
        }
        TermEvent poll(int timeoutMs) override {
            wtimeout(stdscr, timeoutMs);
            int key = wgetch(stdscr);
            if (key == ERR) {
#if !defined(_WIN32) && !defined(WORDLE_CURSES_PDCURSES)
                pollfd descriptor {STDIN_FILENO, POLLIN, 0};
                if (::poll(&descriptor, 1, 0) > 0 && (descriptor.revents & (POLLHUP | POLLERR | POLLNVAL))) {
                    return {TermEvent::Type::Close};
                }
#endif
                return {};
            }
            if (key == KEY_RESIZE) {
#ifdef WORDLE_CURSES_PDCURSES
                resize_term(0, 0);
#endif
                return {TermEvent::Type::Resize};
            }
            if (key == KEY_MOUSE) {
                MEVENT event {};
                if (getmouse(&event) != OK) return {};
                return {
                    TermEvent::Type::Mouse,
                    0,
                    event.x,
                    event.y,
                    bool(event.bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON3_PRESSED | BUTTON3_CLICKED)),
                    bool(event.bstate & (BUTTON3_PRESSED | BUTTON3_CLICKED))
                };
            }
            switch (key) {
            case KEY_UP:
                key = Key::Up;
                break;
            case KEY_DOWN:
                key = Key::Down;
                break;
            case KEY_LEFT:
                key = Key::Left;
                break;
            case KEY_RIGHT:
                key = Key::Right;
                break;
#ifdef WORDLE_CURSES_PDCURSES
            case CTL_LEFT:
                key = Key::CtrlLeft;
                break;
            case CTL_RIGHT:
                key = Key::CtrlRight;
                break;
#elif defined(NCURSES_VERSION)
            case cursesCtrlLeft:
                key = Key::CtrlLeft;
                break;
            case cursesCtrlRight:
                key = Key::CtrlRight;
                break;
#endif
            case KEY_DC:
                key = Key::Delete;
                break;
            case KEY_HOME:
                key = Key::Home;
                break;
            case KEY_END:
                key = Key::End;
                break;
            case KEY_PPAGE:
                key = Key::PageUp;
                break;
            case KEY_NPAGE:
                key = Key::PageDown;
                break;
            case KEY_BACKSPACE:
            case 127:
                key = '\b';
                break;
            case KEY_ENTER:
            case '\n':
                key = '\r';
                break;
            default:
                if (key >= KEY_MIN) return {};
            }
            return {TermEvent::Type::Key, key};
        }
        void draw(int x, int y, const TermCell& cell) override {
            chtype attributes = A_NORMAL;
            if (COLORS < 16 && termColor(cell.foreground) && cell.foreground >= 8 && cell.foreground < 16) {
                attributes |= A_BOLD;
            }
            if (cell.styles & Bold) attributes |= A_BOLD;
            if (cell.styles & Dim) attributes |= A_DIM;
            if (cell.styles & Underline) attributes |= A_UNDERLINE;
            if (cell.styles & Blink) attributes |= A_BLINK;
            if (cell.styles & Reverse) attributes |= A_REVERSE;
#ifdef A_ITALIC
            if (cell.styles & Italic) attributes |= A_ITALIC;
#endif
            if (has_colors()) {
                const bool basicColors = termColor(cell.foreground) &&
                    termColor(cell.background) &&
                    (!directColors() || keepAnsi16());
                const bool extendedColors = directColors() && !basicColors;
                auto colors =
                    std::make_pair(palette(cell.foreground, basicColors), palette(cell.background, basicColors));
                auto pairKey = colors;
                if (extendedColors) { pairKey.first |= 0x40000000; }
                auto found = pairs.find(pairKey);
                if (found == pairs.end() && nextPair < COLOR_PAIRS && nextPair <= SHRT_MAX) {
                    int result;
                    if (!extendedColors) {
                        result = init_pair(
                            (short) nextPair,
                            (short) colors.first,
                            (short) colors.second
                        );
                    }
                    else {
#if defined(NCURSES_VERSION) || defined(WORDLE_PDCURSES_EXTENDED)
                        result = init_extended_pair(nextPair, colors.first, colors.second);
#else
                        result = ERR;
#endif
                    }
                    if (result == ERR) {
                        throw std::runtime_error(
                            !extendedColors ? "curses color pair initialization failed"
                                            : "curses extended color pair initialization failed"
                        );
                    }
                    found = pairs.emplace(pairKey, nextPair++).first;
                }
                else if (found == pairs.end()) { throw std::runtime_error("curses color pair slots exhausted"); }
                if (found != pairs.end()) attributes |= COLOR_PAIR(found->second);
            }
            wattrset(stdscr, attributes);
            std::wstring text;
            char32_t cp = 0;
            int remaining = 0;
            for (unsigned char byte : cell.text) {
                if (byte < 128) {
                    cp = byte;
                    remaining = 0;
                }
                else if ((byte & 0xe0) == 0xc0) {
                    cp = byte & 0x1f;
                    remaining = 1;
                    continue;
                }
                else if ((byte & 0xf0) == 0xe0) {
                    cp = byte & 0x0f;
                    remaining = 2;
                    continue;
                }
                else if ((byte & 0xf8) == 0xf0) {
                    cp = byte & 0x07;
                    remaining = 3;
                    continue;
                }
                else {
                    cp = (cp << 6) | (byte & 0x3f);
                    if (--remaining) continue;
                }
                if (sizeof(wchar_t) == 2 && cp > 0xffff) text += L'?';
                else text += (wchar_t) cp;
            }
            mvwaddwstr(stdscr, y, x, text.c_str());
        }
        void present() override {
            wnoutrefresh(stdscr);
            doupdate();
        }
        void cursor(bool visible) override { curs_set(visible ? 1 : 0); }
    };
} // namespace

std::unique_ptr<Terminal> create_curses_term(const TermOptions& options) {
    return std::make_unique<CursesTerminal>(options);
}
