#include "registry.hpp"
#include "game_context.hpp"

#include <exception>

#include "util/dialog.hpp"
#include "i18n.hpp"
#include "eventbus.hpp"
#include "logger.hpp"
#include "options.hpp"

Result GameContext::accept(std::string input) {
    const auto displayInput = input;
    input = dict->normalize(std::move(input));
    if (!dict->valid(input)) return Result::INVALID;
    try {
        if (validate && !dict->acceptable.count(input)) return Result::INVALID;
        auto result = grader->func(input, answer);
        if (result.size() != 5) throw std::runtime_error(tr(Msg::HintInvalidResult));
        HistoryType history;
        history.input = displayInput;
        for (int i = 0; i < 5; i++) {
            int sid = result[i];
            auto it = grader->ruleset.find(sid);
            if (it == grader->ruleset.end()) throw std::runtime_error(tr(Msg::HintInvalidResult));
            history.colors[i] = it->second.color;
            if (it->second.overrides.count(state[input[i] - 'a'])) {
                state[input[i] - 'a'] = sid;
                color[input[i] - 'a'] = it->second.isSpoiler ? options->colors.background : it->second.color;
            }
        }
        this->history.push_back(history);
        return input == answer ? Result::CORRECT : Result::INCORRECT;
    }
    catch (const std::exception& e) {
        logger.write(Logger::Error, "JUDGE", "评测时出错：" + answer + " <=> " + input + " - " + e.what());
        confirm(64, 8, tr(msg::ErrorGrader {e.what()}), []() { evbus->send(Events::Main {}); });
        return Result::FAILED;
    }
}

GameContext::GameContext(
    const GraderType& grader, const Dictionary& dict,
    std::string dictId, std::string graderId,
    bool validate, bool answerOnly, bool showAlphabet, int maxGuesses
) : dictId(std::move(dictId)), graderId(std::move(graderId)), grader(&grader), dict(&dict),
    validate(validate), showAlphabet(showAlphabet), maxGuesses(maxGuesses) {
    answer = random_word(answerOnly ? dict.answers : dict.acceptable);
    for (int i = 0; i < 26; i++) state[i] = -1, color[i] = options->colors.foreground;
    if (this->grader->start) this->grader->start();
}

GameContext::~GameContext() {
    if (!grader || !grader->finish) return;
    try {
        grader->finish();
    }
    catch (const std::exception& e) {
        logger.write(
            Logger::Error, "JUDGE", "结束评测时出错：" + dictId + " / " + graderId + " - " + e.what()
        );
    }
}

GameContext* g_context;
