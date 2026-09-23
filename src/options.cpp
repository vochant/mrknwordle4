#include "options.hpp"
#include "json_schema.hpp"
#include "locale.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <limits>
#include <cctype>

using nlohmann::json;

namespace {
    const json& section(const json& parent, const char* name) {
        static const json empty = json::object();
        auto found = parent.find(name);
        if (found == parent.end()) return empty;
        if (!found->is_object()) throw std::invalid_argument(std::string(name) + " must be an object");
        return *found;
    }

    CursesAnsi16 parse_curses_ansi16(const json& value) {
        if (value.is_boolean()) return value.get<bool>() ? CursesAnsi16::On : CursesAnsi16::Off;
        if (!value.is_string()) throw std::invalid_argument("terminal.cursesAnsi16 must be auto, on or off");
        const auto mode = value.get<std::string>();
        if (mode == "auto") return CursesAnsi16::Auto;
        if (mode == "on" || mode == "true") return CursesAnsi16::On;
        if (mode == "off" || mode == "false") return CursesAnsi16::Off;
        throw std::invalid_argument("terminal.cursesAnsi16 must be auto, on or off");
    }

    Wal parse_wal(const json& value) {
        if (value.is_number_integer()) {
            switch (value.get<int>()) {
            case 0:
                return Wal::Off;
            case 1:
                return Wal::Checkpoint;
            case 2:
                return Wal::Keep;
            }
        }
        else if (value.is_string()) {
            const auto mode = value.get<std::string>();
            if (mode == "off") return Wal::Off;
            if (mode == "checkpoint") return Wal::Checkpoint;
            if (mode == "keep") return Wal::Keep;
        }
        throw std::invalid_argument("storage.wal must be off, checkpoint or keep");
    }

} // namespace

Options::Options(const json& document) {
    checkSchema(document);
    const auto& config = document;
    check_fields(config, {
        "schemaVersion", "defaults", "game", "terminal",
        "locale", "storage", "dictionary", "plugins", "colors"
    });
    if (!config.contains("colors")) throw std::invalid_argument("colors is required");
    const auto& colorsConfig = section(config, "colors");
    check_fields(colorsConfig, {
        "foreground", "background", "muted", "selected",
        "hover", "inputActive", "inputPlaceholder", "error",
        "dictionaryImpossible", "accept", "answer"
    });
    for (const auto* field : {
        "foreground", "background", "muted", "selected",
        "hover", "inputActive", "inputPlaceholder", "error",
        "dictionaryImpossible", "accept", "answer"
    }) {
        if (!colorsConfig.contains(field)) {
            throw std::invalid_argument(std::string("colors.") + field + " is required");
        }
    }
    colors.foreground = parse_color(colorsConfig.at("foreground").get<std::string>());
    colors.background = parse_color(colorsConfig.at("background").get<std::string>());
    colors.muted = parse_color(colorsConfig.at("muted").get<std::string>());
    colors.selected = parse_color(colorsConfig.at("selected").get<std::string>());
    colors.hover = parse_color(colorsConfig.at("hover").get<std::string>());
    colors.inputActive = parse_color(colorsConfig.at("inputActive").get<std::string>());
    colors.inputPlaceholder = parse_color(colorsConfig.at("inputPlaceholder").get<std::string>());
    colors.error = parse_color(colorsConfig.at("error").get<std::string>());
    colors.dictionaryImpossible = parse_color(colorsConfig.at("dictionaryImpossible").get<std::string>());
    colors.accept = parse_color(colorsConfig.at("accept").get<std::string>());
    colors.answer = parse_color(colorsConfig.at("answer").get<std::string>());

    const auto& defaultsConfig = section(config, "defaults");
    check_fields(defaultsConfig, {"dictionary", "grader"});
    defaults.dictionary = defaultsConfig.value("dictionary", defaults.dictionary);
    defaults.grader = defaultsConfig.value("grader", defaults.grader);

    const auto& gameConfig = section(config, "game");
    check_fields(gameConfig, {"answerOnly", "validation", "maxGuesses", "showAlphabet"});
    if (gameConfig.contains("maxGuesses") && (
        !gameConfig.at("maxGuesses").is_number_integer() ||
        gameConfig.at("maxGuesses") < -1 ||
        gameConfig.at("maxGuesses") > std::numeric_limits<int>::max()
    )) {
        throw std::invalid_argument("game.maxGuesses must be an integer from -1 to INT_MAX");
    }
    game.answerOnly = gameConfig.value("answerOnly", game.answerOnly);
    game.validation = gameConfig.value("validation", game.validation);
    game.limit = gameConfig.value("maxGuesses", game.limit);
    game.charmap = gameConfig.value("showAlphabet", game.charmap);
    if (game.limit < -1) throw std::invalid_argument("game.maxGuesses must be -1 or greater");

    const auto& termConfig = section(config, "terminal");
    check_fields(termConfig, {
        "backend", "mouse", "handleInterrupts", "cursesTerm",
        "cursesAnsi16", "cursesReservedColors"
    });
    term.backend = termConfig.value("backend", term.backend);
    if (term.backend != "auto" &&
        term.backend != "ansi" &&
        term.backend != "curses" &&
        term.backend != "win32" &&
        term.backend != "win32-vt"
    ) {
        throw std::invalid_argument("Unknown terminal.backend: " + term.backend);
    }
    term.mouse = termConfig.value("mouse", term.mouse);
    term.handleInterrupt = termConfig.value("handleInterrupts", term.handleInterrupt);
    term.cursesTerm = termConfig.value("cursesTerm", term.cursesTerm);
    if (termConfig.contains("cursesAnsi16")) {
        term.cursesAnsi16 = parse_curses_ansi16(termConfig.at("cursesAnsi16"));
    }
    term.cursesReservedColors = termConfig.value("cursesReservedColors", term.cursesReservedColors);
    if (term.cursesTerm.size() > 128 || !std::all_of(
        term.cursesTerm.begin(), term.cursesTerm.end(),
        [](unsigned char character) {
            return std::isalnum(character) ||
            character == '+' ||
            character == '-' ||
            character == '_' ||
            character == '.';
        }
    )) {
        throw std::invalid_argument("terminal.cursesTerm must be an empty or valid terminfo name");
    }
    if (term.cursesReservedColors < 0 || term.cursesReservedColors > 256) {
        throw std::invalid_argument("terminal.cursesReservedColors must be an integer from 0 to 256");
    }
    check_fields(section(config, "storage"), {"wal"});
    const auto& storageConfig = section(config, "storage");
    if (storageConfig.contains("wal")) storage.wal = parse_wal(storageConfig.at("wal"));

    const auto& localeConfig = section(config, "locale");
    check_fields(localeConfig, {"language", "timeZone", "calendar"});
    locale.language = normalize_lang(localeConfig.value("language", locale.language));
    locale.timeZone = localeConfig.value("timeZone", locale.timeZone);
    locale.calendar = localeConfig.value("calendar", locale.calendar);
    if (locale.language.empty()) throw std::invalid_argument("locale.language cannot be empty");

    const auto& dictConfig = section(config, "dictionary");
    check_fields(dictConfig, {"validation", "cleanupOnExit", "showIds", "showImpossible", "search", "answerOnly"});
    dict.validation = dictConfig.value("validation", dict.validation);
    dict.cleanup = dictConfig.value("cleanupOnExit", dict.cleanup);
    dict.showId = dictConfig.value("showIds", dict.showId);
    dict.showImpossible = dictConfig.value("showImpossible", dict.showImpossible);
    const auto& search = section(dictConfig, "search");
    check_fields(search, {"enabled", "default", "engines"});
    dict.search = search.value("enabled", dict.search);
    dict.searchEngine = search.value("default", dict.searchEngine);
    dict.searchEngines = section(search, "engines").get<std::map<std::string, std::string>>();
    for (const auto& [id, url] : dict.searchEngines) {
        auto placeholder = url.find("%s");
        if (id.empty() ||
            placeholder == std::string::npos ||
            url.find("%s", placeholder + 2) != std::string::npos ||
            url.find("https://") != 0 && url.find("http://") != 0
        ) {
            throw std::invalid_argument("Search URL must be HTTP(S) with exactly one %s: " + id);
        }
    }
    if (dict.search && !dict.searchEngines.count(dict.searchEngine)) dict.search = false;
    dict.answerOnly = dictConfig.value("answerOnly", dict.answerOnly);

    const auto& plugin = section(config, "plugins");
    check_fields(plugin, {"enabled", "load", "runtimes"});
    plugins.enabled = plugin.value("enabled", plugins.enabled);
    const auto runtimes = plugin.value("runtimes", json::array());
    if (!runtimes.is_array()) throw std::invalid_argument("plugins.runtimes must be an array");
    for (const auto& item : runtimes) {
        auto backend = item.get<std::string>();
        if (backend != "lua") throw std::invalid_argument("Unknown plugin backend: " + backend);
        if (!plugins.runtimes.insert(backend).second) {
            throw std::invalid_argument("Duplicate plugin backend: " + backend);
        }
    }
    std::set<std::string> ids;
    const auto load = plugin.value("load", json::array());
    if (!load.is_array()) throw std::invalid_argument("plugins.load must be an array");
    for (const auto& item : load) {
        check_fields(item, {"id", "path"});
        auto id = item.at("id").get<std::string>();
        if (id.empty() || !ids.insert(id).second) throw std::invalid_argument("Empty or duplicate plugin id: " + id);
        plugins.sources.push_back({id, item.at("path").get<std::string>()});
    }
}

const Options* options = nullptr;
