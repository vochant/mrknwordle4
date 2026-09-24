#include "registry.hpp"
#include "options.hpp"
#include "i18n.hpp"

#include <algorithm>
#include <random>
#include <stdexcept>

std::string random_word(const WordSet& dictionary) {
    thread_local std::mt19937 random(std::random_device {}());
    std::uniform_int_distribution<size_t> distribution(0, dictionary.size() - 1);
    auto word = dictionary.begin();
    std::advance(word, distribution(random));
    return *word;
}

Registry::Registry() {
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
