#pragma once

#include "color.hpp"
#include "dictionary.hpp"
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include <nlohmann/json_fwd.hpp>

struct GameOptions {
    bool answerOnly = true;
    bool validation = true;
    int limit = -1;
    bool charmap = true;
};

enum class CursesAnsi16 { Auto, On, Off };
enum class Wal { Off, Checkpoint, Keep };

struct TermOptions {
    std::string backend = "auto";
    bool mouse = false;
    bool handleInterrupt = true;
    std::string cursesTerm;
    CursesAnsi16 cursesAnsi16 = CursesAnsi16::Auto;
    int cursesReservedColors = 256;
};

struct LocaleOptions {
    std::string language = "en-US";
    std::string timeZone = "local";
    std::string calendar = "default";
};

struct StorageOptions {
    Wal wal = Wal::Off;
};

struct DictionaryOptions {
    bool validation = false;
    bool cleanup = true;
    bool showId = true;
    bool showImpossible = false;
    bool answerOnly = false;
    std::map<std::string, std::string> searchEngines;
    bool search = true;
    std::string searchEngine = "google";
};

struct DefaultsOptions {
    std::string dictionary = "core.english";
    std::string grader = "core.wordle";
};

struct PluginSource {
    std::string id;
    std::filesystem::path directory;
};

struct PluginOptions {
    bool enabled = true;
    std::vector<PluginSource> sources;
    std::set<std::string> runtimes;
};

struct HighlightOptions {
    Color foreground;
    Color background;
    Color muted;
    Color selected;
    Color hover;
    Color inputActive;
    Color inputPlaceholder;
    Color error;
    Color dictionaryImpossible;
    Color accept;
    Color answer;
};

struct Options {
    DefaultsOptions defaults;
    GameOptions game;
    TermOptions term;
    LocaleOptions locale;
    StorageOptions storage;
    DictionaryOptions dict;
    PluginOptions plugins;
    HighlightOptions colors;

    explicit Options(const nlohmann::json& config);
};

extern const Options* options;
