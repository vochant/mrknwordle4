#include "dictionary.hpp"
#include "json_schema.hpp"

std::string Dictionary::normalize(std::string word) const {
    for (auto& ch : word) {
        if (ch >= 'A' && ch <= 'Z') ch += 'a' - 'A';
        auto equiv = equivalents.find(ch);
        if (equiv != equivalents.end()) ch = equiv->second;
    }
    return word;
}

WordSet Dictionary::normalize(const WordSet& words) const {
    WordSet res;
    for (const auto& word : words) {
        if (std::any_of(word.begin(), word.end(), [&](char letter) { return isBanned(letter); })) continue;
        res.insert(normalize(word));
    }
    return res;
}

bool Dictionary::valid(const std::string& word) const {
    return word.size() == 5 && std::all_of(word.begin(), word.end(), [&](char letter) {
        return letter >= 'a' && letter <= 'z' && !isBanned(letter);
    });
}

namespace {
    Dictionary parse_dict_base(const nlohmann::json& spec) {
        using nlohmann::json;
        Dictionary res;
        res.name = spec.at("name").get<std::string>();
        if (res.name.empty()) throw std::invalid_argument("Dictionary name cannot be empty");
        const auto& equivs = spec.at("equivalents");
        if (!equivs.is_array()) throw std::invalid_argument("equivalents must be an array of character classes");
        std::set<char> assigned;
        for (const auto& value : equivs) {
            auto group = value.get<std::string>();
            if (group.size() < 2) throw std::invalid_argument("Equivalent classes require at least two characters");
            bool ban = group.find('!') != std::string::npos;
            char canonical = 0;
            size_t letters = 0;
            for (char ch : group) {
                if ((ch < 'a' || ch > 'z') && ch != '!') {
                    throw std::invalid_argument("Equivalent classes accept only lowercase ASCII letters and !");
                }
                if (!assigned.insert(ch).second) {
                    throw std::invalid_argument("A character can occur in only one equivalent class");
                }
                if (ch != '!') {
                    if (!canonical) canonical = ch;
                    letters++;
                }
            }
            if (!letters) throw std::invalid_argument("A banned equivalent class must contain a letter");
            for (char ch : group) {
                if (ch == '!') continue;
                if (ban) res.banned.insert(ch);
                else if (ch != canonical) res.equivalents.emplace(ch, canonical);
            }
        }
        return res;
    }

} // namespace

Dictionary parse_dict(const nlohmann::json& spec) {
    check_fields(spec, {"name", "equivalents"});
    return parse_dict_base(spec);
}
