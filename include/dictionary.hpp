#pragma once

#include "color.hpp"
#include <map>
#include <set>
#include <string>
#include <vector>
#include <nlohmann/json_fwd.hpp>

using WordSet = std::set<std::string>;

struct Dictionary {
    std::string name;
    WordSet acceptable, answers;
    std::map<char, char> equivalents;
    std::set<char> banned;

    std::string normalize(std::string word) const;
    WordSet normalize(const WordSet& words) const;
    bool valid(const std::string& word) const;
    bool isAlias(char letter) const { return equivalents.count(letter) != 0; }
    bool isBanned(char letter) const { return banned.count(letter) != 0; }
};

struct WordRule {
    std::string filter;
    std::string target;
    WordSet words;
};

Dictionary parse_dict(const nlohmann::json& spec);
