#include "terminal/terminal.hpp"
#include "options.hpp"

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <stdexcept>
#include <unicode/uchar.h>
#include <unicode/unistr.h>

std::unique_ptr<Terminal> term;
TermScreen termScreen;

namespace {
    volatile std::sig_atomic_t interrupted = 0;
    using SignalHandler = void (*)(int);
    SignalHandler prevInterrupt, prevTerminate;
    void onSignal(int) { interrupted = 1; }
} // namespace

void install_term_signals() {
    interrupted = 0;
    prevInterrupt = std::signal(SIGINT, onSignal);
    prevTerminate = std::signal(SIGTERM, onSignal);
}

void restore_term_signals() {
    std::signal(SIGINT, prevInterrupt);
    std::signal(SIGTERM, prevTerminate);
}

bool term_interrupted() { return interrupted != 0; }

bool TermCell::operator==(const TermCell& other) const {
    return text == other.text &&
        foreground == other.foreground &&
        background == other.background &&
        width == other.width &&
        styles == other.styles;
}

void TermScreen::setBackground(int color) {
    background = color < 0 ? 0 : color;
    cells.assign(size_t(1) * columns * rows, blank());
    prev = cells;
    invalid = true;
    dirty = true;
}

void TermScreen::resize(int width, int height) {
    if (width == columns && height == rows) return;
    columns = std::max(0, width);
    rows = std::max(0, height);
    cells.assign(size_t(1) * columns * rows, blank());
    prev = cells;
    invalid = true;
    dirty = true;
}

void TermScreen::eraseGlyph(int x, int y) {
    auto ix = size_t(1) * y * columns + x;
    if (cells[ix].width == 0 && x > 0) cells[ix - 1] = blank();
    else if (cells[ix].width == 2 && x + 1 < columns) cells[ix + 1] = blank();
    cells[ix] = blank();
}

void TermScreen::put(int x, int y, TermCell cell) {
    if (y < 0 || y >= rows || x < 0 || x >= columns ||
        cell.width != 1 && cell.width != 2 ||
        x + cell.width > columns
    ) return;
    if (cells[size_t(1) * y * columns + x] == cell) return;
    dirty = true;
    eraseGlyph(x, y);
    if (cell.width == 2) {
        eraseGlyph(x + 1, y);
        cells[size_t(1) * y * columns + x + 1] = {"", cell.foreground, cell.background, 0, cell.styles};
    }
    cells[size_t(1) * y * columns + x] = std::move(cell);
}

TermCell TermScreen::cell(int x, int y) const {
    if (x < 0 || x >= columns || y < 0 || y >= rows) return blank();
    return cells[size_t(1) * y * columns + x];
}

int TermScreen::prevColumn(int x, int y) const {
    if (x <= 0) return 0;
    int column = x - 1;
    if (y >= 0 && y < rows && column < columns &&
        cells[size_t(1) * y * columns + column].width == 0 &&
        column > 0
    ) column--;
    return column;
}

void TermScreen::invalidate() { invalid = true; }

void TermScreen::present(Terminal& backend) {
    if (!invalid && !dirty) return;
    bool changed = false;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < columns; j++) {
            const auto ix = size_t(1) * i * columns + j;
            const auto& cell = cells[ix];
            if (cell.width && (invalid || !(cell == prev[ix]))) {
                backend.draw(j, i, cell);
                changed = true;
            }
        }
    }
    if (changed) backend.present();
    prev = cells;
    invalid = false;
    dirty = false;
}

std::unique_ptr<Terminal> create_term(const TermOptions& configured) {
    const char* overrideBackend = std::getenv("WORDLE_BACKEND");
    std::string backend = overrideBackend ? overrideBackend : configured.backend;
    if (backend == "auto") {
#ifdef WORDLE_HAS_CURSES
        try { return create_curses_term(configured); } catch (const std::exception&) {}
#endif
#if defined(WORDLE_HAS_ANSI)
        try { return create_ansi_term(configured.mouse); } catch (const std::exception&) {}
#endif
#if defined(WORDLE_HAS_WIN32)
        try { return create_win32_vt_term(configured.mouse); } catch (const std::exception&) {}
        return create_win32_term(configured.mouse);
#endif
    }
#ifdef WORDLE_HAS_CURSES
    if (backend == "curses") return create_curses_term(configured);
#endif
#ifdef WORDLE_HAS_WIN32
    if (backend == "win32") return create_win32_term(configured.mouse);
    if (backend == "win32-vt") return create_win32_vt_term(configured.mouse);
#endif
#ifdef WORDLE_HAS_ANSI
    if (backend == "ansi") return create_ansi_term(configured.mouse);
#endif
    throw std::runtime_error("Terminal backend unavailable: " + backend);
}
