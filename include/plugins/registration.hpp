#pragma once

#include "dictionary.hpp"
#include "game_types.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

struct ActiveGrader {
    std::string id;
    nlohmann::json definition;
    GraderFunc check;
    GraderCompatibleFunc compatible;
    GraderLifecycleFunc start, finish;
};

struct ActiveDictionary {
    std::string id;
    Dictionary dictionary;
};

struct ActiveWords {
    std::string id;
    WordRule rule;
};

struct ActivePluginResources {
    std::vector<ActiveGrader> graders;
    std::vector<ActiveDictionary> dictionaries;
    std::vector<ActiveWords> words;
};
