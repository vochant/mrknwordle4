#include "terminal/terminal.hpp"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <algorithm>
#include <cstdio>
#include <deque>
#include <limits>
#include <string>
#include <stdexcept>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#ifndef DISABLE_NEWLINE_AUTO_RETURN
#define DISABLE_NEWLINE_AUTO_RETURN 0x0008
#endif

namespace {
    class Win32Terminal final : public Terminal {
        HANDLE input = INVALID_HANDLE_VALUE, output = INVALID_HANDLE_VALUE, original = INVALID_HANDLE_VALUE;
        DWORD inputMode = 0;
        DWORD prevButtons = 0;
        bool mouseEnabled;
        int columns = 0, rows = 0;
        int defaultBackground = 0;
        std::vector<CHAR_INFO> buffer;
        int left = 0, top = 0, right = -1, bottom = -1;
        bool enableVt = false;
        bool vmouse = false;
        std::wstring vout;
        std::string vin;
        int vx = -1, vy = -1, vforeground = -1, vbackground = -1;
        uint16_t vstyles = 0xffff;
        std::deque<TermEvent> inputEvents;
        TermEvent pendingMouse;
        static int palette(int color) { return nearest_term_color(color); }
        static std::wstring vtColor(int value, bool background) {
            if (isRgbColor(value)) {
                const int rgb = rgbValue(value);
                return std::wstring(background ? L"48;2;" : L"38;2;") +
                    std::to_wstring((rgb >> 16) & 255) + L";" +
                    std::to_wstring((rgb >> 8) & 255) + L";" +
                    std::to_wstring(rgb & 255);
            }
            int index = ((value & 4) >> 2) | (value & 2) | ((value & 1) << 2);
            return std::to_wstring((background ? 40 : 30) + (value & 8 ? 60 : 0) + index);
        }
        TermEvent decodeVirtualInput() {
            if (vin.empty()) return {};
            if (vin.front() != '\x1b') {
                int key = (unsigned char) vin.front();
                vin.erase(0, 1);
                if (key == '\n') key = '\r';
                if (key == 127) key = '\b';
                return {TermEvent::Type::Key, key};
            }
            if (vin.size() == 1) return {};
            if (vin[1] != '[' && vin[1] != 'O') {
                vin.erase(0, 1);
                return {TermEvent::Type::Key, Key::Escape};
            }
            size_t end = 2;
            while (end < vin.size() && !(vin[end] >= '@' && vin[end] <= '~')) ++end;
            if (end == vin.size()) return {};
            const std::string seq = vin.substr(2, end - 2);
            const char final = vin[end];
            vin.erase(0, end + 1);
            if (!seq.empty() && seq.front() == '<') {
                int button = 0, column = 0, row = 0;
                if (std::sscanf(seq.c_str(), "<%d;%d;%d", &button, &column, &row) == 3) {
                    bool click = final == 'M' && !(button & (32 | 64)) && (button & 3) != 3;
                    return {TermEvent::Type::Mouse, 0, column - 1, row - 1, click, (button & 3) == 2};
                }
                return {};
            }
            int key = 0;
            switch (final) {
            case 'A':
                key = Key::Up;
                break;
            case 'B':
                key = Key::Down;
                break;
            case 'C':
                key = seq == "1;5" ? Key::CtrlRight : Key::Right;
                break;
            case 'D':
                key = seq == "1;5" ? Key::CtrlLeft : Key::Left;
                break;
            case 'H':
                key = Key::Home;
                break;
            case 'F':
                key = Key::End;
                break;
            case '~':
                if (seq == "3") key = Key::Delete;
                else if (seq == "1" || seq == "7") key = Key::Home;
                else if (seq == "4" || seq == "8") key = Key::End;
                else if (seq == "5") key = Key::PageUp;
                else if (seq == "6") key = Key::PageDown;
                break;
            }
            return key ? TermEvent {TermEvent::Type::Key, key} : TermEvent {};
        }
        int measureGlyphWidth(const std::string& text) const override {
            auto* self = const_cast<Win32Terminal*>(this);
            const COORD origin {0, 0};
            if (!SetConsoleCursorPosition(self->output, origin)) return 0;
            termScreen.invalidate();
            self->vx = self->vy = -1;
            self->left = self->top = 0;
            self->right = self->bottom = -1;
            int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), (int) text.size(), nullptr, 0);
            if (length <= 0) return 0;
            std::wstring probe(length, L' ');
            if (MultiByteToWideChar(CP_UTF8, 0, text.data(), (int) text.size(), probe.data(), length) != length) return 0;
            DWORD written = 0;
            if (!WriteConsoleW(self->output, probe.data(), (DWORD) probe.size(), &written, nullptr) || written != probe.size()) return 0;
            CONSOLE_SCREEN_BUFFER_INFO info {};
            if (!GetConsoleScreenBufferInfo(self->output, &info)) return 0;
            int width = info.dwCursorPosition.Y == origin.Y ? info.dwCursorPosition.X - origin.X : 0;
            FillConsoleOutputCharacterW(self->output, L' ', 2, origin, &written);
            SetConsoleCursorPosition(self->output, origin);
            self->left = self->top = 0;
            self->right = self->bottom = -1;
            return width == 1 || width == 2 ? width : 0;
        }
        void enqueueKey(const KEY_EVENT_RECORD& event) {
            if (enableVt && event.uChar.UnicodeChar) {
                if (event.uChar.UnicodeChar > 127 && vin.empty()) {
                    inputEvents.push_back({TermEvent::Type::Key, event.uChar.UnicodeChar});
                    return;
                }
                vin.push_back((char) event.uChar.UnicodeChar);
                while (true) {
                    auto decoded = decodeVirtualInput();
                    if (decoded.type == TermEvent::Type::None) return;
                    inputEvents.push_back(decoded);
                }
            }
            int key = event.uChar.UnicodeChar;
            bool control = event.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED);
            switch (event.wVirtualKeyCode) {
            case VK_UP:
                key = Key::Up;
                break;
            case VK_DOWN:
                key = Key::Down;
                break;
            case VK_LEFT:
                key = control ? Key::CtrlLeft : Key::Left;
                break;
            case VK_RIGHT:
                key = control ? Key::CtrlRight : Key::Right;
                break;
            case VK_DELETE:
                key = Key::Delete;
                break;
            case VK_HOME:
                key = Key::Home;
                break;
            case VK_END:
                key = Key::End;
                break;
            case VK_PRIOR:
                key = Key::PageUp;
                break;
            case VK_NEXT:
                key = Key::PageDown;
                break;
            }
            if (!key || (event.uChar.UnicodeChar > 127 && key == event.uChar.UnicodeChar)) return;
            const DWORD repeats = std::max<DWORD>(event.wRepeatCount, 1);
            for (DWORD repeat = 0; repeat < repeats; ++repeat) inputEvents.push_back({TermEvent::Type::Key, key});
        }
        bool collectInput() {
            DWORD pending = 0;
            if (!GetNumberOfConsoleInputEvents(input, &pending)) return false;
            for (DWORD remaining = std::min<DWORD>(pending, 4096); remaining; --remaining) {
                INPUT_RECORD record {};
                DWORD count;
                if (!ReadConsoleInputW(input, &record, 1, &count)) return false;
                if (record.EventType == WINDOW_BUFFER_SIZE_EVENT) {
                    buffer.clear();
                    right = bottom = -1;
                    vx = vy = -1;
                    pendingMouse = {};
                    inputEvents.push_back({TermEvent::Type::Resize});
                }
                else if (record.EventType == MOUSE_EVENT && mouseEnabled) {
                    const auto& event = record.Event.MouseEvent;
                    DWORD pressed = event.dwButtonState & ~prevButtons;
                    prevButtons = event.dwButtonState;
                    CONSOLE_SCREEN_BUFFER_INFO info {};
                    GetConsoleScreenBufferInfo(output, &info);
                    TermEvent mouse {
                        TermEvent::Type::Mouse,
                        0,
                        event.dwMousePosition.X - info.srWindow.Left,
                        event.dwMousePosition.Y - info.srWindow.Top,
                        bool(pressed),
                        bool(pressed & RIGHTMOST_BUTTON_PRESSED)
                    };
                    if (pressed) {
                        pendingMouse = {};
                        inputEvents.push_back(mouse);
                    }
                    else pendingMouse = mouse;
                }
                else if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
                    enqueueKey(record.Event.KeyEvent);
                }
            }
            return true;
        }
        TermEvent takeQueuedInput() {
            if (inputEvents.empty()) return {};
            auto event = inputEvents.front();
            inputEvents.pop_front();
            return event;
        }

    public:
        bool singleBmpCells() const override { return !enableVt; }
        Win32Terminal(bool mouse, bool preferVirtualTerminal) : mouseEnabled(mouse) {
            input = GetStdHandle(STD_INPUT_HANDLE);
            original = GetStdHandle(STD_OUTPUT_HANDLE);
            DWORD outputMode;
            if (!GetConsoleMode(input, &inputMode) || !GetConsoleMode(original, &outputMode)) {
                throw std::runtime_error("win32 backend requires a Windows console; try curses for other terminals");
            }
            output = CreateConsoleScreenBuffer(
                GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, CONSOLE_TEXTMODE_BUFFER,
                nullptr
            );
            if (output == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot create console screen buffer");
            CONSOLE_SCREEN_BUFFER_INFO originalInfo {};
            if (GetConsoleScreenBufferInfo(original, &originalInfo)) {
                SetConsoleScreenBufferSize(output, originalInfo.dwSize);
                SMALL_RECT window {0, 0,
                    (SHORT) (originalInfo.srWindow.Right - originalInfo.srWindow.Left),
                    (SHORT) (originalInfo.srWindow.Bottom - originalInfo.srWindow.Top)
                };
                SetConsoleWindowInfo(output, TRUE, &window);
            }
            if (!SetConsoleActiveScreenBuffer(output)) {
                CloseHandle(output);
                throw std::runtime_error("Cannot activate console screen buffer");
            }
            DWORD createdOutputMode = 0;
            if (preferVirtualTerminal && GetConsoleMode(output, &createdOutputMode)) {
                enableVt = SetConsoleMode(
                    output,
                    createdOutputMode |
                    ENABLE_VIRTUAL_TERMINAL_PROCESSING |
                    DISABLE_NEWLINE_AUTO_RETURN
                ) != 0;
            }
            if (!SetConsoleMode(input, (
                inputMode & ~(
                    ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_QUICK_EDIT_MODE |
                    ENABLE_VIRTUAL_TERMINAL_INPUT |ENABLE_MOUSE_INPUT
                )
            ) | ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | (
                mouse && !enableVt ? ENABLE_MOUSE_INPUT : 0
            ) | (
                enableVt ? ENABLE_VIRTUAL_TERMINAL_INPUT : 0
            ))) {
                SetConsoleActiveScreenBuffer(original);
                CloseHandle(output);
                throw std::runtime_error("Cannot set console input mode");
            }
            vmouse = mouse && enableVt;
            if (vmouse) {
                const wchar_t enableMouse[] = L"\x1b[?1003h\x1b[?1006h";
                DWORD written = 0;
                WriteConsoleW(
                    output, enableMouse,
                    (DWORD) (sizeof(enableMouse) / sizeof(*enableMouse) - 1),
                    &written, nullptr
                );
            }
            cursor(false);
        }
        ~Win32Terminal() override {
            if (vmouse) {
                const wchar_t disableMouse[] = L"\x1b[?1003l\x1b[?1006l";
                DWORD written = 0;
                WriteConsoleW(
                    output, disableMouse,
                    (DWORD) (sizeof(disableMouse) / sizeof(*disableMouse) - 1),
                    &written, nullptr
                );
            }
            SetConsoleMode(input, inputMode);
            SetConsoleActiveScreenBuffer(original);
            CloseHandle(output);
        }
        std::pair<int, int> size() override {
            CONSOLE_SCREEN_BUFFER_INFO info {};
            GetConsoleScreenBufferInfo(output, &info);
            return {info.srWindow.Right - info.srWindow.Left + 1, info.srWindow.Bottom - info.srWindow.Top + 1};
        }
        TermEvent poll(int timeoutMs) override {
            if (enableVt) {
                auto decoded = decodeVirtualInput();
                if (decoded.type != TermEvent::Type::None) return decoded;
            }
            if (auto event = takeQueuedInput(); event.type != TermEvent::Type::None) return event;
            if (WaitForSingleObject(input, 0) == WAIT_OBJECT_0 && !collectInput()) return {TermEvent::Type::Close};
            if (auto event = takeQueuedInput(); event.type != TermEvent::Type::None) return event;
            if (pendingMouse.type != TermEvent::Type::None) {
                auto event = pendingMouse;
                pendingMouse = {};
                return event;
            }
            if (WaitForSingleObject(input, timeoutMs) != WAIT_OBJECT_0) return {};
            if (!collectInput()) return {TermEvent::Type::Close};
            if (auto event = takeQueuedInput(); event.type != TermEvent::Type::None) return event;
            if (pendingMouse.type != TermEvent::Type::None) {
                auto event = pendingMouse;
                pendingMouse = {};
                return event;
            }
            return {};
        }
        void draw(int x, int y, const TermCell& cell) override {
            if (enableVt) {
                if (x != vx || y != vy) {
                    vout += L"\x1b[" + std::to_wstring(y + 1) + L";" + std::to_wstring(x + 1) + L"H";
                }
                if (vforeground != cell.foreground ||
                    vbackground != cell.background ||
                    vstyles != cell.styles
                ) {
                    vout += L"\x1b[0;" + vtColor(cell.foreground, false) + L";" + vtColor(cell.background, true);
                    for (auto [flag, code] : {
                        std::pair<int, int> {Bold, 1},
                        {Dim, 2},
                        {Italic, 3},
                        {Underline, 4},
                        {Blink, 5},
                        {Reverse, 7},
                        {Strike, 9}}
                    ) {
                        if (cell.styles & flag) {
                            vout += L";" + std::to_wstring(code);
                        }
                    }
                    vout += L"m";
                    vforeground = cell.foreground;
                    vbackground = cell.background;
                    vstyles = cell.styles;
                }
                int length = MultiByteToWideChar(
                    CP_UTF8, MB_ERR_INVALID_CHARS,
                    cell.text.data(), (int) cell.text.size(),
                    nullptr, 0
                );
                if (length <= 0) throw std::logic_error("Win32 VT draw requires valid UTF-8");
                std::wstring text(length, L' ');
                MultiByteToWideChar(
                    CP_UTF8, MB_ERR_INVALID_CHARS,
                    cell.text.data(), (int) cell.text.size(),
                    text.data(), length
                );
                vout += text;
                vx = x + cell.width;
                vy = y;
                return;
            }
            if (buffer.empty()) {
                auto dimensions = size();
                columns = dimensions.first;
                rows = dimensions.second;
                CHAR_INFO empty {};
                empty.Char.UnicodeChar = L' ';
                const int color = palette(defaultBackground);
                empty.Attributes = (WORD) (color | (color << 4));
                buffer.assign(size_t(1) * columns * rows, empty);
            }
            if (x < 0 || y < 0 || x + cell.width > columns || y >= rows) return;
            std::wstring text(MultiByteToWideChar(
                CP_UTF8, 0,
                cell.text.data(), (int) cell.text.size(),
                nullptr, 0
            ), L' ');
            MultiByteToWideChar(
                CP_UTF8, 0,
                cell.text.data(), (int) cell.text.size(),
                text.data(), (int) text.size()
            );
            if (text.size() != 1 || (text.front() >= 0xd800 && text.front() <= 0xdfff)) {
                throw std::logic_error("Win32 draw requires a prepared single-BMP glyph");
            }
            int foreground = palette(cell.foreground), background = palette(cell.background);
            if (cell.styles & Reverse) std::swap(foreground, background);
            WORD attributes = (WORD) (foreground | (background << 4));
            if (cell.styles & Underline) attributes |= COMMON_LVB_UNDERSCORE;
            auto index = size_t(1) * y * columns + x;
            buffer[index].Char.UnicodeChar = text.front();
            buffer[index].Attributes = attributes;
            if (cell.width == 2) {
                buffer[index].Attributes |= COMMON_LVB_LEADING_BYTE;
                buffer[index + 1].Char.UnicodeChar = buffer[index].Char.UnicodeChar;
                buffer[index + 1].Attributes = attributes | COMMON_LVB_TRAILING_BYTE;
            }
            if (right < 0) {
                left = x;
                top = y;
                right = x + cell.width - 1;
                bottom = y;
            }
            else {
                left = std::min(left, x);
                top = std::min(top, y);
                right = std::max(right, x + cell.width - 1);
                bottom = std::max(bottom, y);
            }
        }
        void present() override {
            if (enableVt) {
                size_t offset = 0;
                while (offset < vout.size()) {
                    DWORD written = 0;
                    if (!WriteConsoleW(
                        output,
                        vout.data() + offset, (DWORD) (vout.size() - offset),
                        &written, nullptr
                    ) || !written) {
                        break;
                    }
                    offset += written;
                }
                vout.clear();
                return;
            }
            if (right < 0) return;
            CONSOLE_SCREEN_BUFFER_INFO info {};
            GetConsoleScreenBufferInfo(output, &info);
            SMALL_RECT region {
                (SHORT) (left + info.srWindow.Left), (SHORT) (top + info.srWindow.Top),
                (SHORT) (right + info.srWindow.Left), (SHORT) (bottom + info.srWindow.Top)
            };
            WriteConsoleOutputW(
                output, buffer.data(),
                {(SHORT) columns, (SHORT) rows},
                {(SHORT) left, (SHORT) top},
                &region
            );
            right = bottom = -1;
        }
        void cursor(bool visible) override {
            CONSOLE_CURSOR_INFO info {};
            GetConsoleCursorInfo(output, &info);
            info.bVisible = visible;
            SetConsoleCursorInfo(output, &info);
        }
        void setBackground(int value) override {
            defaultBackground = value < 0 ? 0 : value;
            if (enableVt) {
                vout += L"\x1b[0;37;" + vtColor(defaultBackground, true) + L"m\x1b[2J\x1b[H";
                vx = vy = vforeground = vbackground = -1;
                vstyles = 0xffff;
                present();
                return;
            }
            if (buffer.empty()) return;
            const int color = palette(defaultBackground);
            const WORD attributes = (WORD) (color | (color << 4));
            for (auto& cell : buffer) {
                cell.Char.UnicodeChar = L' ';
                cell.Attributes = attributes;
            }
            left = top = 0;
            right = columns - 1;
            bottom = rows - 1;
        }
        void setTitle(const std::string& title) override {
            SetConsoleTitleA(title.c_str());
        }
    };
} // namespace

std::unique_ptr<Terminal> create_win32_term(bool mouse) { return std::make_unique<Win32Terminal>(mouse, false); }

std::unique_ptr<Terminal> create_win32_vt_term(bool mouse) { return std::make_unique<Win32Terminal>(mouse, true); }
