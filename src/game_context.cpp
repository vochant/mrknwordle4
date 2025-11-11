#include "game_context.hpp"

#include <exception>

#include "util/dialog.hpp"
#include "i18n.hpp"
#include "ipc.hpp"
#include "logger.hpp"

Result GameContext::accept(std::string input) {
    if (!basicValidation(input)) return Result::INVALID;
    if (options->validation && !gamemode->validator(input)) return Result::INVALID;
    try {
        auto result = gamemode->judger->func(input, answer);
        if (result.size() != 5) throw std::runtime_error(translate("{hint.invalid_result}"));
        HistoryType history;
        history.input = input;
        for (int i = 0; i < 5; i++) {
            int sid = result[i];
            auto it = gamemode->judger->ruleset.find(sid);
            if (it == gamemode->judger->ruleset.end()) throw std::runtime_error(translate("{hint.invalid_result}"));
            history.colors[i] = it->second.color;
            if (it->second.overrides.count(state[input[i] - 'a'])) {
                state[input[i] - 'a'] = sid;
                color[input[i] - 'a'] = it->second.isSpoiler ? 0 : it->second.color;
            }
        }
        this->history.push_back(history);
        return input == answer ? Result::CORRECT : Result::INCORRECT;
    }
    catch (const std::exception& e) {
        logger.write(Logger::Error, "JUDGE", "评测时出错：" + answer + " <=> " + input + " - " + e.what());
        confirm(64, 8, translate("{hint.judge_failed}\n") + e.what(), []() {
            ipc->send({"shutdown", 0});
        });
        return Result::FAILED;
    }
}

GameContext::GameContext(Gamemode* gamemode, std::string id) : gamemode(gamemode), id(id) {
    answer = gamemode->problemset();
    for (int i = 0; i < 26; i++) state[i] = -1, color[i] = 15;
}

GameContext* g_context;