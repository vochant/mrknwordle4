#pragma once

#include "game_types.hpp"

struct Options;

class Registry {
public:
    std::map<std::string, Dictionary> dicts;
    std::vector<WordRule> words;
    std::map<std::string, std::shared_ptr<GraderType>> graders;
    std::vector<std::pair<std::string, std::shared_ptr<GraderType>>> deterministicGraders;
    std::vector<std::pair<std::string, std::shared_ptr<GraderType>>> filterGraders;

    Registry();
    void rebuildIndexes();
    void applyWords();
    void validateConfig(const Options& settings) const;
    Color wordColor(const std::string& dictionary, const std::string& word) const;
};

extern Registry* registry;

std::string random_word(const WordSet& dictionary);
