#include "ui/screens.hpp"
#include "widgets/layout.hpp"
#include "widgets/link.hpp"
#include "widgets/switch.hpp"
#include "registry.hpp"
#include "options.hpp"
#include "eventbus.hpp"
#include "dict_context.hpp"
#include "i18n.hpp"

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
    using Choices = std::vector<std::pair<std::string, std::string>>;

    Choices dictionaries() {
        Choices result;
        for (const auto& [id, dictionary] : registry->dicts) result.emplace_back(id, dictionary.name);
        return result;
    }

    int selected(const Choices& choices, const std::string& value) {
        auto found = std::find_if(choices.begin(), choices.end(), [&](const auto& choice) {
            return choice.first == value;
        });
        if (found == choices.end()) throw std::invalid_argument("Unknown setup choice: " + value);
        return found - choices.begin();
    }

    struct GameState {
        Choices dictionaries, graders;
        int dictionary = 0, grader = 0;
        int initialDictionary = 0, initialGrader = 0;
        bool answerPool = false, validate = false;
        Link* dictionaryLink = nullptr;
        Link* graderLink = nullptr;
        Switch* answerPoolSwitch = nullptr;
        Switch* validateSwitch = nullptr;
    };

    struct DictionaryState {
        Choices dictionaries, graders;
        int dictionary = 0, grader = 0;
        int initialDictionary = 0, initialGrader = 0;
        bool answerPool = false, validate = false, impossible = false;
        Link* dictionaryLink = nullptr;
        Link* graderLink = nullptr;
        Switch* answerPoolSwitch = nullptr;
        Switch* validateSwitch = nullptr;
        Switch* impossibleSwitch = nullptr;
    };

    std::shared_ptr<GameState> gameState;
    std::shared_ptr<DictionaryState> dictionaryState;
} // namespace

std::unique_ptr<Widget> GameMenu() {
    gameState = std::make_shared<GameState>();
    const auto state = gameState;
    state->dictionaries = dictionaries();
    for (const auto& [id, grader] : registry->graders) state->graders.emplace_back(id, grader->name);
    state->dictionary = state->initialDictionary = selected(state->dictionaries, options->defaults.dictionary);
    state->grader = state->initialGrader = selected(state->graders, options->defaults.grader);
    state->answerPool = !options->game.answerOnly;
    state->validate = options->game.validation;

    auto updateDictionary = [state]() {
        state->dictionaryLink->set(tr(msg::FieldValue {
            tr(Msg::SetupDictionary) + " (D)", state->dictionaries.at(state->dictionary).second
        }), 77);
    };
    auto updateGrader = [state]() {
        state->graderLink->set(tr(msg::FieldValue {
            tr(Msg::SetupGrader) + " (G)", state->graders.at(state->grader).second
        }), 77);
    };

    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Main {});
    }));

    auto dictionary = std::make_unique<Link>(0, 2, 77, "", 'd', [state, updateDictionary]() {
        state->dictionary = (state->dictionary + 1) % state->dictionaries.size();
        updateDictionary();
    });
    state->dictionaryLink = dictionary.get();
    updateDictionary();
    layout->add(std::move(dictionary));

    auto grader = std::make_unique<Link>(0, 3, 77, "", 'g', [state, updateGrader]() {
        state->grader = (state->grader + 1) % state->graders.size();
        updateGrader();
    });
    state->graderLink = grader.get();
    updateGrader();
    layout->add(std::move(grader));

    layout->add(std::make_unique<Link>(0, 4, 77, tr(Msg::SetupAdvanced), 'a', []() {
        evbus->send(Events::Route {"gameAdvanced"});
    }));
    layout->add(std::make_unique<Link>(0, 6, 77, tr(Msg::SetupConfirm), '\r', [state]() {
        evbus->send(Events::GameStart {
            state->graders.at(state->grader).first, state->dictionaries.at(state->dictionary).first,
            state->validate, !state->answerPool, options->game.charmap, options->game.limit
        });
    }));

    return layout;
}

void resetGameSetup() {
    gameState->dictionary = gameState->initialDictionary;
    gameState->grader = gameState->initialGrader;
    gameState->answerPool = !options->game.answerOnly;
    gameState->validate = options->game.validation;
    gameState->dictionaryLink->set(tr(msg::FieldValue {
        tr(Msg::SetupDictionary) + " (D)", gameState->dictionaries.at(gameState->dictionary).second
    }), 77);
    gameState->graderLink->set(tr(msg::FieldValue {
        tr(Msg::SetupGrader) + " (G)", gameState->graders.at(gameState->grader).second
    }), 77);
    gameState->answerPoolSwitch->set(gameState->answerPool);
    gameState->validateSwitch->set(gameState->validate);
}

std::unique_ptr<Widget> GameAdvancedMenu() {
    const auto state = gameState;
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"gameEntry"});
    }));
    const auto pool = tr(Msg::SetupAnswerPool) + " (P)";
    auto poolSwitch = std::make_unique<Switch>(0, 2, 77, pool, pool, 'p', [state](bool value) {
        state->answerPool = value;
    }, state->answerPool);
    state->answerPoolSwitch = poolSwitch.get();
    layout->add(std::move(poolSwitch));
    const auto validate = tr(Msg::SetupValidate) + " (V)";
    auto validateSwitch = std::make_unique<Switch>(0, 3, 77, validate, validate, 'v', [state](bool value) {
        state->validate = value;
    }, state->validate);
    state->validateSwitch = validateSwitch.get();
    layout->add(std::move(validateSwitch));
    return layout;
}

std::unique_ptr<Widget> DictSelector() {
    dictionaryState = std::make_shared<DictionaryState>();
    const auto state = dictionaryState;
    state->dictionaries = dictionaries();
    for (const auto& [id, grader] : registry->graders) state->graders.emplace_back(id, grader->name);
    state->dictionary = state->initialDictionary = selected(state->dictionaries, options->defaults.dictionary);
    state->grader = state->initialGrader = selected(state->graders, options->defaults.grader);
    state->answerPool = options->dict.answerOnly;
    state->validate = options->dict.validation;
    state->impossible = options->dict.showImpossible;

    auto updateDictionary = [state]() {
        state->dictionaryLink->set(tr(msg::FieldValue {
            tr(Msg::SetupDictionary) + " (D)", state->dictionaries.at(state->dictionary).second
        }), 77);
    };
    auto updateGrader = [state]() {
        state->graderLink->set(tr(msg::FieldValue {
            tr(Msg::SetupGrader) + " (G)", state->graders.at(state->grader).second
        }), 77);
    };

    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Main {});
    }));

    auto dictionary = std::make_unique<Link>(0, 2, 77, "", 'd', [state, updateDictionary]() {
        state->dictionary = (state->dictionary + 1) % state->dictionaries.size();
        updateDictionary();
    });
    state->dictionaryLink = dictionary.get();
    updateDictionary();
    layout->add(std::move(dictionary));

    auto grader = std::make_unique<Link>(0, 3, 77, "", 'g', [state, updateGrader]() {
        state->grader = (state->grader + 1) % state->graders.size();
        updateGrader();
    });
    state->graderLink = grader.get();
    updateGrader();
    layout->add(std::move(grader));

    layout->add(std::make_unique<Link>(0, 4, 77, tr(Msg::SetupAdvanced), 'a', []() {
        evbus->send(Events::Route {"dictAdvanced"});
    }));
    layout->add(std::make_unique<Link>(0, 6, 77, tr(Msg::SetupConfirm), '\r', [state]() {
        auto context = std::make_unique<DictContext>(
            state->dictionaries.at(state->dictionary).first,
            registry->graders.at(state->graders.at(state->grader).first), state->answerPool, state->validate,
            state->impossible
        );
        delete g_dict_ctxt;
        g_dict_ctxt = context.release();
        evbus->send(Events::UpdateDict {});
        evbus->send(Events::ResetFilter {});
        evbus->send(Events::Route {"dictEntry"});
    }));

    return layout;
}

void resetDictionarySetup() {
    dictionaryState->dictionary = dictionaryState->initialDictionary;
    dictionaryState->grader = dictionaryState->initialGrader;
    dictionaryState->answerPool = options->dict.answerOnly;
    dictionaryState->validate = options->dict.validation;
    dictionaryState->impossible = options->dict.showImpossible;
    dictionaryState->dictionaryLink->set(tr(msg::FieldValue {
        tr(Msg::SetupDictionary) + " (D)", dictionaryState->dictionaries.at(dictionaryState->dictionary).second
    }), 77);
    dictionaryState->graderLink->set(tr(msg::FieldValue {
        tr(Msg::SetupGrader) + " (G)", dictionaryState->graders.at(dictionaryState->grader).second
    }), 77);
    dictionaryState->answerPoolSwitch->set(dictionaryState->answerPool);
    dictionaryState->validateSwitch->set(dictionaryState->validate);
    dictionaryState->impossibleSwitch->set(dictionaryState->impossible);
}

std::unique_ptr<Widget> DictAdvancedSelector() {
    const auto state = dictionaryState;
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"dictInit1"});
    }));
    const auto pool = tr(Msg::SetupViewPool) + " (P)";
    auto poolSwitch = std::make_unique<Switch>(0, 2, 77, pool, pool, 'p', [state](bool value) {
        state->answerPool = value;
    }, state->answerPool);
    state->answerPoolSwitch = poolSwitch.get();
    layout->add(std::move(poolSwitch));
    const auto validate = tr(Msg::SetupValidate) + " (V)";
    auto validateSwitch = std::make_unique<Switch>(0, 3, 77, validate, validate, 'v', [state](bool value) {
        state->validate = value;
    }, state->validate);
    state->validateSwitch = validateSwitch.get();
    layout->add(std::move(validateSwitch));
    const auto impossible = tr(Msg::SetupImpossible) + " (I)";
    auto impossibleSwitch = std::make_unique<Switch>(0, 4, 77, impossible, impossible, 'i', [state](bool value) {
        state->impossible = value;
    }, state->impossible);
    state->impossibleSwitch = impossibleSwitch.get();
    layout->add(std::move(impossibleSwitch));
    return layout;
}
