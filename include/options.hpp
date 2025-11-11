#pragma once

#include <vector>
#include <string>
#include <map>
#include <set>
#include <utility>
#include <nlohmann/json.hpp>
#include <functional>

using namespace nlohmann;

bool basicValidation(const std::string& str);

typedef std::function<std::vector<int>(const std::string&, const std::string&)> JudgerFunc;

struct JudgerType {
    struct JudgerResult {
        char color;
        bool isSpoiler;
        std::set<int> overrides;
    };

    bool determined;

    JudgerFunc func;
    std::map<int, JudgerResult> ruleset;
};

typedef std::set<std::string> Dictionary;
typedef std::function<bool(const std::string&)> ValidatorType;
typedef std::function<std::string()> ProblemsetType;

struct Gamemode {
    JudgerType* judger;
    ValidatorType validator;
    ProblemsetType problemset;
    std::string displayName;
};

#define PLUGIN_DICTIONARIES 3 // PLUGINS_WORDS is included
#define PLUGIN_WORDS 2
#define PLUGIN_SEARCH_ENGINES 4
#define PLUGIN_JUDGERS 8
#define PLUGIN_GAMEMODES 16
#define PLUGIN_PROBLEMSETS 32
#define PLUGIN_VALIDATORS 64
#define PLUGIN_LANGUAGES 128

struct Options {
    std::map<std::string, Dictionary> dictionaries;
    Dictionary fullDictionary;
    bool validation;
    int limit;
    bool virtualTerminal;
    bool mouseControlling;
    bool charmap;
    int codepage;
    bool handleInterrupt;
    short walType;
    std::string language;
    std::string calendar;

    bool dictValidation;
    bool dictCleanup;
    std::vector<std::pair<std::string, char>> dictHighlights;
    bool dictShowId;
    bool dictShowImpossible;
    char dictImpossibleColor;
    std::map<std::string, std::string> dictSearchEngines;
    bool dictSearch;
    std::string dictSearchEngine;

    bool pluginEnabled;
    bool pluginScripting;
    bool pluginDynamicLibraries;
    bool pluginSystem;
    bool pluginPrependLocation;
    bool pluginIsolated;
    short pluginFeatures;
    std::string pluginManifest;
    std::map<std::string, std::string> pluginList;
    std::vector<std::pair<std::string, std::string>> pluginInfo;

    std::map<std::string, JudgerType*> judgers;
    std::vector<std::pair<std::string, JudgerType*>> determinedJudgers;
    std::map<std::string, ValidatorType> validators;
    std::map<std::string, ProblemsetType> problemsets;
    std::map<std::string, Gamemode> gamemodes;

    Options(json config);

    void load_plugins();
    void post_load();
};

extern Options* options;