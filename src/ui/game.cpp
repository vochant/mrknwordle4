#include "registry.hpp"
#include "ui/screens.hpp"
#include "eventbus.hpp"
#include "services/keybd.service.hpp"
#include "widgets/input.hpp"
#include "widgets/layout.hpp"
#include "widgets/text.hpp"
#include "game_context.hpp"
#include "i18n.hpp"
#include "logger.hpp"
#include "options.hpp"
#include "userdb.hpp"
#include <sstream>
#include "util/dialog.hpp"

static std::string getAlphabet() {
    std::stringstream ss;
    if (!g_context->showAlphabet) return {};
    for (char letter = 'a'; letter <= 'z'; ++letter) {
        int i = letter - 'a';
        const auto equiv = g_context->dict->equivalents.find(letter);
        const int stateIndex = (
            equiv == g_context->dict->equivalents.end() ?
            letter :
            equiv->second
        ) - 'a';
        auto color = g_context->dict->isBanned(letter) ?
            options->colors.muted : (
                !~g_context->state[stateIndex] ?
                options->colors.foreground :
                g_context->color[stateIndex]
            );
        ss << colorescape(color, options->colors.background);
        ss << (char) (i + 'A');
        if (i == 6 || i == 13 || i == 19) { ss << '\n'; }
        else if (i == 16 || i == 22) { ss << "   "; }
        else if (i != 25) { ss << ' '; }
    }
    return ss.str();
}

static std::string getHistory(int line_size, int max_lines) {
    int start = 0;
    std::stringstream ss;
    if (max_lines && g_context->history.size() > max_lines * line_size) {
        max_lines--;
        ss << colorescape(options->colors.muted, options->colors.background) << "(...)";
        for (int i = 0; i < line_size - 1; i++) ss << "      ";
        ss << '\n';
        start = g_context->history.size() - max_lines * line_size;
    }
    int line = 0;
    for (int i = start; i < g_context->history.size(); i++) {
        auto& obj = g_context->history[i];
        for (int j = 0; j < 5; j++) {
            ss << colorescape(obj.colors[j], options->colors.background) << obj.input[j];
        }
        line++;
        if (i != g_context->history.size() - 1) {
            if (line == line_size) { ss << '\n', line = 0; }
            else { ss << ' '; }
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

std::unique_ptr<Widget> GameMain() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    Text* alphabet = nullptr;
    Text* history = nullptr;
    {
        auto alphabetWidget = std::make_unique<Text>(0, 0, 13, "");
        alphabet = alphabetWidget.get();
        alphabet->h = 14;
        layout->add(std::move(alphabetWidget));
    }
    layout->add(std::make_unique<Text>(26, 0, 35, tr(Msg::HintHistory)));
    auto historyWidget = std::make_unique<Text>(26, 1, 35, "");
    history = historyWidget.get();
    history->h = 20;
    layout->add(std::move(historyWidget));
    layout->add(std::make_unique<Text>(0, 16, 18, tr(msg::HintInput {"Tab"})));
    auto input = std::make_unique<Input>(0, 17, 24, false, "", tr(Msg::HintPleaseInput), [](char32_t ch) -> bool {
        return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
    });
    auto input_hint = std::make_unique<Text>(0, 20, 18, tr(Msg::HintInvalidInput), 0);
    auto* inputPtr = input.get();
    auto* inputHintPtr = input_hint.get();
    layout->add(std::move(input_hint));
    layout->add(std::move(input));

    static bool gameRunning = false;

    evbus->listen<Events::GameStart>([
        alphabet, history,
        input = inputPtr,
        input_hint = inputHintPtr
    ](const Events::GameStart& msg) {
        delete g_context;
        g_context = nullptr;
        gameRunning = false;
        try {
            g_context = new GameContext(
                *registry->graders.at(msg.grader),
                registry->dicts.at(msg.dictionary),
                msg.dictionary, msg.grader, msg.validate,
                msg.answerOnly, msg.showAlphabet, msg.maxGuesses
            );
        }
        catch (const std::exception& e) {
            logger.write(Logger::Error, "JUDGE", "开始评测时出错：" + msg.grader + " - " + e.what());
            confirm(64, 8, tr(msg::ErrorGrader {e.what()}), []() {
                evbus->send(Events::Main {});
            });
            return;
        }
        alphabet->set(getAlphabet(), 13);
        history->set(colorescape(options->colors.muted, options->colors.background) + tr(Msg::HintEmptyHistory), 35);
        input->set("");
        input_hint->color(options->colors.background);
        gameRunning = true;
        evbus->send(Events::Route {"game"});
    });

    inputPtr->setSubmit([
        alphabet, history,
        input = inputPtr,
        input_hint = inputHintPtr
    ]() {
        if (!gameRunning) return;
        auto text = input->get();
        auto result = g_context->accept(text);
        if (result == Result::FAILED) {
            delete g_context;
            g_context = nullptr;
            gameRunning = false;
            return;
        }
        if (result == Result::INVALID) {
            input_hint->color(options->colors.error);
            return;
        }
        input_hint->color(options->colors.background);
        input->set("");
        alphabet->set(getAlphabet(), 13);
        history->set(getHistory(6, 20), 35);
        if (result == Result::CORRECT) {
            write_history(
                g_context->answer,
                getHistory(13, 15),
                g_context->history.size(),
                g_context->dictId,
                g_context->graderId
            );
            evbus->send(Events::UpdateHistory {});
            delete g_context;
            g_context = nullptr;
            gameRunning = false;
            confirm(40, 8, tr(Msg::HintWin), []() { evbus->send(Events::Main {}); });
        }
        else if (g_context->maxGuesses > 0 && (int) g_context->history.size() >= g_context->maxGuesses) {
            write_history(g_context->answer, getHistory(13, 15), -1, g_context->dictId, g_context->graderId);
            evbus->send(Events::UpdateHistory {});
            delete g_context;
            g_context = nullptr;
            gameRunning = false;
            confirm(40, 8, tr(Msg::HintLose), []() { evbus->send(Events::Main {}); });
        }
    });

    return layout;
}
