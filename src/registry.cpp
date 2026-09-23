#include "registry.hpp"
#include "options.hpp"
#include "i18n.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>

namespace {
    std::vector<int> wordle_grader(const std::string& guess, const std::string& answer) {
        std::vector<int> result(5, 0);
        bool ignore[5] {};
        short appears[26] {};
        for (short i = 0; i < 5; i++) ignore[i] = false;
        for (short i = 0; i < 26; i++) appears[i] = 0;
        for (short i = 0; i < 5; i++) {
            if (guess[i] == answer[i]) {
                result[i] = 2;
                ignore[i] = true;
            }
            else appears[answer[i] - 'a']++;
        }
        for (short i = 0; i < 5; i++) {
            if (appears[guess[i] - 'a'] && !ignore[i]) {
                result[i] = 1;
                appears[guess[i] - 'a']--;
            }
        }
        return result;
    }

    std::vector<int> letter_presence_grader(const std::string& guess, const std::string& answer) {
        std::vector<int> result(5, 0);
        short appears[26] {};
        for (short i = 0; i < 26; i++) appears[i] = 0;
        for (short i = 0; i < 5; i++) appears[answer[i] - 'a']++;
        for (short i = 0; i < 5; i++) {
            if (appears[guess[i] - 'a']) {
                result[i] = 1;
                appears[guess[i] - 'a']--;
            }
        }
        return result;
    }

    std::vector<int> match_count_grader(const std::string& guess, const std::string& answer) {
        const auto wordle = letter_presence_grader(guess, answer);
        std::vector<int> result(5, 0);
        const auto cnt = std::count(wordle.begin(), wordle.end(), 1);
        std::fill_n(result.begin(), cnt, 1);
        return result;
    }

    std::vector<int> hardle_grader(const std::string& guess, const std::string& answer) {
        const auto wordle = wordle_grader(guess, answer);
        std::vector<int> result(5, 0);
        const auto green = std::count(wordle.begin(), wordle.end(), 2);
        const auto yellow = std::count(wordle.begin(), wordle.end(), 1);
        std::fill_n(result.begin(), green, 2);
        std::fill_n(result.begin() + green, yellow, 1);
        return result;
    }

} // namespace

std::string random_word(const WordSet& dictionary) {
    thread_local std::mt19937 random(std::random_device {}());
    std::uniform_int_distribution<size_t> distribution(0, dictionary.size() - 1);
    auto word = dictionary.begin();
    std::advance(word, distribution(random));
    return *word;
}

Registry::Registry() {
    auto wordle = std::make_shared<GraderType>();
    wordle->name = "Wordle";
    wordle->func = wordle_grader;
    wordle->deterministic = true;
    wordle->ruleset = {
        {0, {parse_color("term:RGBL"), true, {-1}}},
        {1, {parse_color("term:RGL"), false, {-1, 0}}},
        {2, {parse_color("term:GL"), false, {-1, 0, 1}}},
    };
    graders.insert({"core.wordle", wordle});
    auto letterPresence = std::make_shared<GraderType>();
    letterPresence->name = "Letter presence";
    letterPresence->func = letter_presence_grader;
    letterPresence->deterministic = true;
    letterPresence->ruleset = {
        {0, {parse_color("term:RGBL"), true, {-1}}},
        {1, {parse_color("term:GBL"), false, {-1, 0}}},
    };
    graders.insert({"core.letter_presence", letterPresence});
    auto matchCount = std::make_shared<GraderType>();
    matchCount->name = "Match count";
    matchCount->func = match_count_grader;
    matchCount->deterministic = true;
    matchCount->ruleset = {
        {0, {parse_color("term:RGBL"), false, {}}},
        {1, {parse_color("term:RB"), false, {}}},
    };
    graders.insert({"core.match_count", matchCount});
    auto hardle = std::make_shared<GraderType>();
    hardle->name = "Hardle";
    hardle->func = hardle_grader;
    hardle->deterministic = true;
    hardle->ruleset = {
        {0, {parse_color("term:RGBL"), false, {}}},
        {1, {parse_color("term:RGL"), false, {}}},
        {2, {parse_color("term:GL"), false, {}}},
    };
    graders.insert({"core.hardle", hardle});
    rebuildIndexes();
}

void Registry::rebuildIndexes() {
    deterministicGraders.clear();
    filterGraders.clear();
    for (const auto& [id, grader] : graders) {
        if (grader->deterministic) deterministicGraders.emplace_back(id, grader);
        if (grader->deterministic || grader->hasCompatible) filterGraders.emplace_back(id, grader);
    }
}

void Registry::applyWords() {
    for (auto& [dictionaryId, dictionary] : dicts) {
        for (const auto& rule : words) {
            if (rule.filter != "*" && rule.filter != dictionaryId) continue;
            auto normalized = dictionary.normalize(rule.words);
            dictionary.acceptable.insert(normalized.begin(), normalized.end());
            if (rule.target == "answer") dictionary.answers.insert(normalized.begin(), normalized.end());
        }
    }
}

Registry* registry = nullptr;

void Registry::validateConfig(const Options& settings) const {
    if (!dicts.count(settings.defaults.dictionary) ||
        !graders.count(settings.defaults.grader) ||
        !graders.at(settings.defaults.grader)->deterministic
    ) {
        throw std::invalid_argument("Unknown default dictionary or grader");
    }
}

Color Registry::wordColor(const std::string& id, const std::string& word) const {
    return dicts.at(id).answers.count(word) ? options->colors.answer : options->colors.accept;
}
