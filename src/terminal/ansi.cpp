#include "terminal/terminal.hpp"

#include <cerrno>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <stdexcept>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

using namespace std::chrono;

namespace {
    class AnsiTerminal final : public Terminal {
        termios saved {};
        bool mouseEnabled;
        std::string output, input;
        std::pair<int, int> dims;
        int outputX = -1, outputY = -1, foreground = -1, background = -1;
        uint16_t styles = 0xffff;
        std::chrono::steady_clock::time_point escapeStarted {};

        static std::string color(int value, bool background) {
            if (isRgbColor(value)) {
                const int rgb = rgbValue(value);
                return std::string(background ? "48;2;" : "38;2;") +
                    std::to_string((rgb >> 16) & 255) + ";" +
                    std::to_string((rgb >> 8) & 255) + ";" + std::to_string(rgb & 255);
            }
            int index = ((value & 4) >> 2) | (value & 2) | ((value & 1) << 2);
            return std::to_string((background ? 40 : 30) + (value & 8 ? 60 : 0) + index);
        }

        TermEvent decode() {
            if (input.empty()) return {};
            if (input.front() != '\x1b') {
                int key = (unsigned char) input.front();
                input.erase(0, 1);
                if (key == 127) key = '\b';
                if (key == '\n') key = '\r';
                return {TermEvent::Type::Key, key};
            }
            if (input.size() == 1) return {};
            if (input[1] != '[' && input[1] != 'O') {
                input.erase(0, 1);
                return {TermEvent::Type::Key, Key::Escape};
            }
            size_t end = 2;
            while (end < input.size() && !(input[end] >= '@' && input[end] <= '~')) ++end;
            if (end == input.size()) return {};
            std::string seq = input.substr(2, end - 2);
            char final = input[end];
            input.erase(0, end + 1);
            if (!seq.empty() && seq.front() == '<') {
                int button, column, row;
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

        int probeAmbiguousWidth() {
            output = "\x1b[1;1H│\x1b[6n";
            present();
            const auto deadline = steady_clock::now() + milliseconds(150);
            while (steady_clock::now() < deadline) {
                for (
                    size_t start = input.find("\x1b[");
                    start != std::string::npos;
                    start = input.find("\x1b[", start + 1)
                ) {
                    int row = 0, column = 0, length = 0;
                    if (std::sscanf(
                        input.c_str() + start,
                        "\x1b[%d;%dR%n",
                        &row, &column, &length
                    ) == 2 && length > 0) {
                        input.erase(start, length);
                        if (!input.empty()) escapeStarted = steady_clock::now();
                        return row == 1 && (column == 2 || column == 3) ? column - 1 : 0;
                    }
                }
                auto rem = duration_cast<milliseconds>(deadline - steady_clock::now()).count();
                pollfd fd {STDIN_FILENO, POLLIN, 0};
                int res = ::poll(&fd, 1, (int) std::max<int64_t>(0, rem));
                if (res <= 0 || !(fd.revents & POLLIN)) break;
                char buf[64];
                ssize_t cnt = ::read(STDIN_FILENO, buf, sizeof(buf));
                if (cnt <= 0) break;
                input.append(buf, cnt);
            }
            return 0;
        }

    public:
        explicit AnsiTerminal(bool mouse) : mouseEnabled(mouse) {
            const char* term = std::getenv("TERM");
            if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO) || !term || std::strcmp(term, "dumb") == 0) {
                throw std::runtime_error("ansi backend requires an interactive terminal");
            }
            if (tcgetattr(STDIN_FILENO, &saved) != 0) {
                throw std::runtime_error("Cannot read tty attributes");
            }
            termios raw = saved;
            raw.c_lflag &= ~(ICANON | ECHO | IEXTEN);
            raw.c_iflag &= ~(IXON | ICRNL);
            raw.c_cc[VMIN] = 0;
            raw.c_cc[VTIME] = 0;
            if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
                throw std::runtime_error("Cannot set tty attributes");
            }
            dims = size();
            output = "\x1b[?1049h\x1b[?25l\x1b[?7l\x1b[2J";
            if (mouseEnabled) output += "\x1b[?1003h\x1b[?1006h";
            present();
            int ambiguousWidth = probeAmbiguousWidth();
            setAmb(ambiguousWidth);
            output = "\x1b[2J\x1b[H";
            present();
        }
        ~AnsiTerminal() override {
            output = mouseEnabled ? "\x1b[?1003l\x1b[?1006l" : "";
            output += "\x1b[0m\x1b[?7h\x1b[?25h\x1b[?1049l";
            present();
            tcsetattr(STDIN_FILENO, TCSANOW, &saved);
        }
        std::pair<int, int> size() override {
            winsize dims {};
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &dims) != 0) return {80, 25};
            return {dims.ws_col, dims.ws_row};
        }
        TermEvent poll(int timeoutMs) override {
            auto current = size();
            if (current != dims) {
                dims = current;
                outputX = outputY = -1;
                return {TermEvent::Type::Resize};
            }
            auto event = decode();
            if (event.type != TermEvent::Type::None) return event;
            pollfd fd {STDIN_FILENO, POLLIN, 0};
            int res = ::poll(&fd, 1, input.empty() ? timeoutMs : std::min(timeoutMs, 30));
            if (res < 0 && errno != EINTR) return {TermEvent::Type::Close};
            if (res > 0 && (fd.revents & POLLIN)) {
                char buf[256];
                ssize_t cnt = ::read(STDIN_FILENO, buf, sizeof(buf));
                if (cnt <= 0) return {TermEvent::Type::Close};
                if (input.empty()) escapeStarted = steady_clock::now();
                input.append(buf, cnt);
            }
            else if (res > 0 && (fd.revents & (POLLHUP | POLLERR | POLLNVAL))) {
                return {TermEvent::Type::Close};
            }
            event = decode();
            if (event.type != TermEvent::Type::None) return event;
            if (!input.empty() && (
                input.size() > 256 ||
                steady_clock::now() - escapeStarted >= milliseconds(30)
            )) {
                bool loneEscape = input.size() == 1;
                input.clear();
                if (loneEscape) return {TermEvent::Type::Key, Key::Escape};
            }
            return {};
        }
        void draw(int x, int y, const TermCell& cell) override {
            if (x != outputX || y != outputY)
                output += "\x1b[" + std::to_string(y + 1) + ";" + std::to_string(x + 1) + "H";
            if (foreground != cell.foreground || background != cell.background || styles != cell.styles) {
                output += "\x1b[0;" + color(cell.foreground, false) + ";" + color(cell.background, true);
                for (auto[flag, code] : {
                    std::pair<int, int> {Bold, 1},
                    {Dim, 2},
                    {Italic, 3},
                    {Underline, 4},
                    {Blink, 5},
                    {Reverse, 7},
                    {Strike, 9}
                }) {
                    if (cell.styles & flag) {
                        output += ";" + std::to_string(code);
                    }
                }
                output += "m";
                styles = cell.styles;
                foreground = cell.foreground;
                background = cell.background;
            }
            output += cell.text;
            outputX = std::all_of(
                cell.text.begin(), cell.text.end(),
                [](unsigned char byte) { return byte >= 32 && byte < 127; }
            ) ? x + cell.width  : -1;
            outputY = y;
        }
        void present() override {
            size_t offset = 0;
            while (offset < output.size()) {
                ssize_t count = ::write(STDOUT_FILENO, output.data() + offset, output.size() - offset);
                if (count < 0 && errno == EINTR) continue;
                if (count <= 0) break;
                offset += count;
            }
            output.clear();
        }
        void cursor(bool visible) override {
            output += visible ? "\x1b[?25h" : "\x1b[?25l";
            present();
        }
        void setTitle(const std::string& title) override {
            output += "\x1b]0;" + title + "\x07";
            present();
        }
    };
} // namespace

bool ansi_term_available() {
    const char* term = std::getenv("TERM");
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) && term && std::strcmp(term, "dumb") != 0;
}

std::unique_ptr<Terminal> create_ansi_term(bool mouse) { return std::make_unique<AnsiTerminal>(mouse); }
