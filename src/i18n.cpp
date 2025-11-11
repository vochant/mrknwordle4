#include "i18n.hpp"

#include <vector>
#include <queue>
#include <cassert>
#include <sstream>
#include <map>
#include <fstream>
#include <nlohmann/json.hpp>

using namespace nlohmann;

class Translator {
private:
    struct Entries {
        unsigned int entries[256];

        unsigned int& operator[](unsigned char c) {
            return entries[c];
        }

        const unsigned int& operator[](unsigned char c) const {
            return entries[c];
        }

        Entries() { for (unsigned int i = 0; i < 256; i++) entries[i] = 0; }
    };

    unsigned int num_nodes, num_strings;
    std::vector<Entries> nodes;
    std::vector<bool> is_end;
    std::vector<unsigned int> valueId;
    std::vector<std::string> valueStore;
    std::vector<unsigned int> depths;

    unsigned int getSub(unsigned int id, char next) {
        if (!nodes[id][next]) {
            nodes[id][next] = num_nodes++;
            nodes.push_back({});
            is_end.push_back(false);
            valueId.push_back(0);
            depths.push_back(depths[id] + 1);
        }
        return nodes[id][next];
    }

public:
    Translator() : num_nodes(1), num_strings(0) {
        nodes.push_back({});
        is_end.push_back(false);
        valueId.push_back(0);
        depths.push_back(0);
    }

    void insert(std::string key, std::string value) {
        int ix = 0;
        for (auto ch : key) {
            ix = getSub(ix, ch);
        }
        assert(!is_end[ix]);
        is_end[ix] = true;
        valueId[ix] = num_strings++;
        valueStore.push_back(value);
    }

    std::string translate(std::string input) {
        unsigned int lastPos = 0, state = 0;
        std::stringstream ss;
        unsigned int len = input.length();
        for (unsigned int i = 0; i < len; i++) {
            char ch = input[i];
            if (nodes[state][ch]) state = nodes[state][ch];
            else state = 0;
            if (is_end[state]) {
                ss << input.substr(lastPos, i - lastPos - depths[state] + 1);
                ss << valueStore[valueId[state]];
                lastPos = i + 1;
                state = 0;
            }
        }
        ss << input.substr(lastPos);
        return ss.str();
    }
};

std::map<std::string, Translator> languages_map;
Translator* current_language = nullptr;

void addTranslation(std::string lang, std::string key, std::string value) {
    if (!languages_map.count(lang)) {
        languages_map.insert({lang, Translator()});
    }
    languages_map[lang].insert(key, value);
}

void loadLanguage(std::string langname, std::filesystem::path langfile) {
    std::ifstream ifs(langfile);
    if (!ifs) return;
    try {
        json lang;
        ifs >> lang;
        if (!lang.is_object() || !lang.contains("content") || !lang["content"].is_array()) return;
        for (auto& item : lang["content"]) {
            if (item.contains("id") && item.contains("content") && item["id"].is_string() && item["content"].is_string()) {
                addTranslation(langname, item["id"], item["content"]);
            }
        }
    }
    catch (const std::exception& e) {
        return;
    }
}

void bindLanguage(std::string lang) {
    if (languages_map.count(lang)) current_language = &languages_map[lang];
    else current_language = nullptr;
}

std::string translate(std::string input) {
    if (!current_language) return input;
    return current_language->translate(input);
}