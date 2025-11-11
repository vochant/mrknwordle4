#include "options.hpp"
#include "ipc.hpp"
#include "services/keybd.service.hpp"
#include "widgets/input.hpp"
#include "widgets/layout.hpp"
#include "widgets/text.hpp"
#include "game_context.hpp"
#include "i18n.hpp"
#include "userdb.hpp"
#include <cctype>
#include <sstream>
#include "util/dialog.hpp"

std::string selectedGamemode;

std::string getAlphabet() {
    std::stringstream ss;
    for (int i = 0; i < 26; i++) {
        ss << '\x01';
        if (!~g_context->state[i]) ss << '\x0f';
        else ss << g_context->color[i];
        ss << (char) (i + 'A');
        if (i == 6 || i == 13 || i == 19) ss << '\n';
        else if (i == 16 || i == 22) ss << "   ";
        else if (i != 25) ss << ' ';
    }
    return ss.str();
}

std::string getHistory(int line_size, int max_lines) {
    int start = 0;
    std::stringstream ss;
    if (max_lines && g_context->history.size() > max_lines * line_size) {
        max_lines--;
        ss << "\x01\x08(...)";
        for (int i = 0; i < line_size - 1; i++) ss << "      ";
        ss << '\n';
        start = g_context->history.size() - max_lines * line_size;
    }
    int current_line = 0;
    for (int i = start; i < g_context->history.size(); i++) {
        auto& obj = g_context->history[i];
        for (int j = 0; j < 5; j++) {
            ss << '\x01' << obj.colors[j] << obj.input[j];
        }
        current_line++;
        if (i != g_context->history.size() - 1) {
            if (current_line == line_size) ss << '\n', current_line = 0;
            else ss << ' ';
        }
    }
    int rem = g_context->history.size() % line_size;
    if (rem) {
        rem = line_size - rem;
        for (int i = 0; i < rem; i++) ss << "      ";
        ss << '\n';
    }
    return ss.str();
}

std::shared_ptr<Widget> GameMain() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    std::shared_ptr<Text> alphabet = nullptr, history = nullptr;
    if (options->charmap) {
        alphabet = std::make_shared<Text>(0, 0, 13, "");
        layout->add(alphabet);
    }
    layout->add(std::make_shared<Text>(26, 0, 35, translate("{hint.history}")));
    history = std::make_shared<Text>(26, 1, 35, "");
    layout->add(history);
    layout->add(std::make_shared<Text>(0, 16, 18, translate("{hint.input} (Tab)")));
    auto input = std::make_shared<Input>(0, 17, 23, false, "", translate("{hint.please_input}"), [](char ch) -> bool {
        return std::islower(ch);
    });
    auto input_hint = std::make_shared<Text>(0, 20, 18, translate("{hint.invalid_input}"), 0);
    layout->add(input_hint);
    layout->add(input);

    static bool gameRunning = false;

    ipc->listen("gameStart", [alphabet, history, input, input_hint](Message msg) {
        g_context = new GameContext(std::any_cast<Gamemode*>(msg.payload), selectedGamemode);
        if (options->charmap) alphabet->set(getAlphabet(), 13);
        history->set(translate("\x01\x08{hint.empty_history}"), 35);
        input->set("");
        input_hint->color(0);
        gameRunning = true;
        ipc->send({"route", std::string("game")});
    });

    keybd_service->listen([alphabet, history, input, input_hint](char ch) {
        if (!gameRunning) return;
        if (ch == '\r') {
            auto text = input->get();
            std::transform(text.begin(), text.end(), text.begin(), [](char ch) -> char { return tolower(ch); });
            auto result = basicValidation(text) ? g_context->accept(text) : Result::INVALID;
            if (result == Result::FAILED) {
                delete g_context;
                gameRunning = false;
                return;
            }
            if (result == Result::INVALID) {
                input_hint->color(12);
                return;
            }
            input_hint->color(0);
            input->set("");
            if (options->charmap) alphabet->set(getAlphabet(), 13);
            history->set(getHistory(6, 20), 35);
            if (result == Result::CORRECT) {
                write_history(g_context->answer, getHistory(13, 15), g_context->history.size(), g_context->id);
                ipc->send({"updateHistory", 0});
                delete g_context;
                gameRunning = false;
                confirm(40, 8, translate("{hint.win}"), []() {
                    ipc->send({"main", 0});
                });
            }
            else if (options->limit > 0 && g_context->history.size() >= options->limit) {
                write_history(g_context->answer, getHistory(13, 15), -1, g_context->id);
                ipc->send({"updateHistory", 0});
                delete g_context;
                gameRunning = false;
                confirm(40, 8, translate("{hint.lose}"), []() {
                    ipc->send({"main", 0});
                });
            }
        }
    });

    return layout;
}