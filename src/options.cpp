#include "options.hpp"
#include "dictreader.hpp"
#include "color.h"
#include "logger.hpp"
#include <unicode/calendar.h>
#include <unicode/locid.h>

#include <random>
#include <exception>
#include <filesystem>
#include <sstream>
#include "i18n.hpp"
#include "mpcc_script.hpp"

using namespace std::filesystem;
using namespace icu;

bool hasCalendar(const char* calName) {
    UErrorCode status = U_ZERO_ERROR;
    Locale loc = Locale("zh_CN", "", (std::string("calendar=") + calName).c_str());
    std::unique_ptr<Calendar> cal(Calendar::createInstance(loc, status));
    return U_SUCCESS(status);
}

char parseColor(std::string color) {
    char result = 0;
    for (auto ch : color) {
        if (ch == 'R') result |= RED;
        if (ch == 'G') result |= GREEN;
        if (ch == 'B') result |= BLUE;
        if (ch == 'L') result |= BRIGHT;
    }
    return result;
}

std::vector<int> commonJudger(const std::string& str, const std::string& answer) {
    std::vector<int> result(5, 0);
    static bool ignore[5];
    static short appears[26];
    for (short i = 0; i < 5; i++) ignore[i] = false;
    for (short i = 0; i < 26; i++) appears[i] = 0;
    for (short i = 0; i < 5; i++) {
        if (str[i] == answer[i]) {
            result[i] = 2;
            ignore[i] = true;
        }
        else {
            appears[answer[i] - 'a']++;
        }
    }
    for (short i = 0; i < 5; i++) {
        if (appears[str[i] - 'a']  && !ignore[i]) {
            result[i] = 1;
            appears[str[i] - 'a']--;
        }
    }
    return result;
}

std::vector<int> lessDetailJudger(const std::string& str, const std::string& answer) {
    std::vector<int> result(5, 0);
    static short appears[26];
    for (short i = 0; i < 26; i++) appears[i] = 0;
    for (short i = 0; i < 5; i++) appears[answer[i] - 'a']++;
    for (short i = 0; i < 5; i++) {
        if (appears[str[i] - 'a'] ) {
            result[i] = 1;
            appears[str[i] - 'a']--;
        }
    }
    return result;
}

std::vector<int> countOnlyJudger(const std::string& str, const std::string& answer) {
    std::vector<int> result(5, 0);
    short count = 0;
    static short appears[26];
    for (short i = 0; i < 26; i++) appears[i] = 0;
    for (short i = 0; i < 5; i++) appears[answer[i] - 'a']++;
    for (short i = 0; i < 5; i++) {
        if (appears[str[i] - 'a'] ) {
            result[count++] = 1;
            appears[str[i] - 'a']--;
        }
    }
    return result;
}

bool commonValidator(const std::string& str) {
    return options->dictionaries["accept"].count(str);
}

bool noValidator(const std::string& str) {
    return true;
}

std::string commonProblemset() {
    static std::mt19937 rnd(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, options->dictionaries["answer"].size() - 1);
    auto it = options->dictionaries["answer"].begin();
    std::advance(it, dist(rnd));
    return *it;
}

std::string hardcoreProblemset() {
    static std::mt19937 rnd(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, options->dictionaries["accept"].size() - 1);
    auto it = options->dictionaries["accept"].begin();
    std::advance(it, dist(rnd));
    return *it;
}

short lookupPluginFeature(const std::string& featureName) {
    if (featureName == "dictionaries") return PLUGIN_DICTIONARIES;
    if (featureName == "words") return PLUGIN_WORDS;
    if (featureName == "searchEngines") return PLUGIN_SEARCH_ENGINES;
    if (featureName == "judgers") return PLUGIN_JUDGERS;
    if (featureName == "gamemodes") return PLUGIN_GAMEMODES;
    if (featureName == "problemsets") return PLUGIN_PROBLEMSETS;
    if (featureName == "validators") return PLUGIN_VALIDATORS;
    if (featureName == "languages") return PLUGIN_LANGUAGES;
    return 0;
}

bool basicValidation(const std::string& str) {
    if (str.length() != 5) return false;
    for (int i = 0; i < 5; i++) if (str[i] < 'a' || str[i] > 'z') return false;
    return true;
}

Options::Options(json Config) {
    if (!Config.contains("core") || !Config["core"].is_string() || Config["core"] != "aho-corasick") {
        throw std::invalid_argument("Invalid core version");
    }
    dictionaries.insert({"answer", {}});
    dictionaries.insert({"accept", {}});
    if (!Config.contains("base") || !Config["base"].is_object()) {
        throw std::invalid_argument("Invalid or missing base configuration");
    }
    auto& config = Config["base"];
    auto it = config.find("answers");
    if (config.contains("answers") && config["answers"].is_array()) {
        for (const auto& item : config["answers"]) {
            if (item.is_object() && item.contains("type") && item.contains("file") && item["type"].is_string() && item["file"].is_string()) {
                std::string type = item["type"];
                std::string file = item["file"];
                auto reader = createDictReader(file, type);
                auto content = reader->read();
                for (auto& word : content) {
                    if (!basicValidation(word)) continue;
                    dictionaries["answer"].insert(word);
                    dictionaries["accept"].insert(word);
                }
            }
        }
    }
    if (config.contains("accepts") && config["accepts"].is_array()) {
        for (const auto& item : config["accepts"]) {
            if (item.is_object() && item.contains("type") && item.contains("file")) {
                std::string type = item["type"];
                std::string file = item["file"];
                auto reader = createDictReader(file, type);
                auto content = reader->read();
                for (auto& word : content) {
                    if (!basicValidation(word)) continue;
                    dictionaries["accept"].insert(word);
                }
            }
        }
    }
    validation = (!config.contains("validation") || !config["validation"].is_boolean() || config["validation"]);
    limit = (config.contains("limit") && config["limit"].is_number_integer()) ? config["limit"].get<int>() : -1;
    if (limit < -1) limit = -1;
    virtualTerminal = (!config.contains("virtualTerminal") || !config["virtualTerminal"].is_boolean() || config["virtualTerminal"]);
    mouseControlling = (config.contains("mouseControlling") && config["mouseControlling"].is_boolean() && config["mouseControlling"]);
    charmap = (!config.contains("charmap") || !config["charmap"].is_boolean() || config["charmap"]);
    codepage = (config.contains("codepage") && config["codepage"].is_number_integer()) ? config["codepage"].get<int>() : -1;
    if (codepage < -1) codepage = -1;
    handleInterrupt = (!config.contains("handleInterrupt") || !config["handleInterrupt"].is_boolean() || config["handleInterrupt"]);
    walType = (config.contains("wal") && config["wal"].is_number_integer()) ? config["wal"].get<short>() : 0;
    if (walType < 0 || walType > 2) walType = 0;
    language = (config.contains("language") && config["language"].is_string()) ? config["language"].get<std::string>() : "en_US.UTF-8";
    if (language == "" || !exists(language + ".lang")) language = "en_US.UTF-8";
    calendar = (config.contains("calendar") && config["calendar"].is_string()) ? config["calendar"].get<std::string>() : "default";
    if (calendar != "default") if (calendar == "" || !hasCalendar(calendar.c_str())) calendar = "default";
    loadLanguage(language, language + ".lang");
    bindLanguage(language);
    if (!Config.contains("dictionary") || !Config["dictionary"].is_object()) {
        throw std::invalid_argument("Invalid or missing dictionary configuration");
    }
    auto& donfig = Config["dictionary"];
    dictValidation = (donfig.contains("validation") && donfig["validation"].is_boolean() && donfig["validation"]);
    dictCleanup = (!donfig.contains("cleanup") || !donfig["cleanup"].is_boolean() || donfig["cleanup"]);
    if (donfig.contains("highlights") && donfig["highlights"].is_array()) {
        for (const auto& item : donfig["highlights"]) {
            if (item.is_object() && item.contains("category") && item.contains("color") && item["category"].is_string() && item["color"].is_string()) {
                std::string category = item["category"];
                char color = parseColor(item["color"]);
                dictHighlights.push_back({category, color});
            }
        }
    }
    dictShowId = (!donfig.contains("showId") || !donfig["showId"].is_boolean() || donfig["showId"]);
    dictShowImpossible = (!donfig.contains("showImpossible") || !donfig["showImpossible"].is_boolean() || donfig["showImpossible"]);
    dictImpossibleColor = (donfig.contains("impossibleColor") && donfig["impossibleColor"].is_string()) ? parseColor(donfig["impossibleColor"]) : BRIGHT;
    if (donfig.contains("searchEngines") && donfig["searchEngines"].is_array()) {
        for (const auto& item : donfig["searchEngines"]) {
            if (item.is_object() && item.contains("id") && item.contains("url") && item["id"].is_string() && item["url"].is_string()) {
                std::string id = item["id"];
                std::string url = item["url"];
                dictSearchEngines.insert({id, url});
            }
        }
    }
    dictSearch = (donfig.contains("search") && donfig["search"].is_boolean() && donfig["search"]);
    dictSearchEngine = (donfig.contains("searchEngine") && donfig["searchEngine"].is_string()) ? donfig["searchEngine"].get<std::string>() : "google";
    if (dictSearch && !dictSearchEngines.count(dictSearchEngine)) dictSearch = false;
    if (!Config.contains("plugins") || !Config["plugins"].is_object()) {
        throw std::invalid_argument("Invalid or missing plugins configuration");
    }
    auto& eonfig = Config["plugins"];
    pluginEnabled = (!eonfig.contains("enabled") || !eonfig["enabled"].is_boolean() || eonfig["enabled"]);
    pluginScripting = (!eonfig.contains("scripting") || !eonfig["scripting"].is_boolean() || eonfig["scripting"]);
    if (pluginScripting) ScriptInit();
    pluginDynamicLibraries = (eonfig.contains("dynamicLibraries") && eonfig["dynamicLibraries"].is_boolean() && eonfig["dynamicLibraries"]);
    pluginSystem = (eonfig.contains("system") && eonfig["system"].is_boolean() && eonfig["system"]);
    pluginPrependLocation = (!eonfig.contains("prependLocation") || !eonfig["prependLocation"].is_boolean() || eonfig["prependLocation"]);
    pluginIsolated = (eonfig.contains("isolated") && eonfig["isolated"].is_boolean() && eonfig["isolated"]);
    pluginFeatures = 0;
    if (eonfig.contains("enableFeatures") && eonfig["enableFeatures"].is_array()) {
        for (const auto& item : eonfig["enableFeatures"]) {
            if (item.is_string()) pluginFeatures |= lookupPluginFeature(item.get<std::string>());
        }
    }
    pluginManifest = (eonfig.contains("manifest") && eonfig["manifest"].is_string()) ? eonfig["manifest"].get<std::string>() : "manifest.json";
    if (eonfig.contains("loadeds") && eonfig["loadeds"].is_array()) {
        for (auto& item : eonfig["loadeds"]) {
            if (item.is_object() && item.contains("directory") && item.contains("id") && item["directory"].is_string() && item["id"].is_string()) {
                std::string directory = item["directory"];
                std::string id = item["id"];
                pluginList.insert({id, directory});
            }
        }
    }
    auto CommonJudger = new JudgerType();
    CommonJudger->func = commonJudger;
    CommonJudger->determined = true;
    CommonJudger->ruleset = {
        {0, {15, true, {-1}}},
        {1, {RED | GREEN | BRIGHT, false, {-1, 0}}},
        {2, {GREEN | BRIGHT, false, {-1, 0, 1}}}
    };
    judgers.insert({"common", CommonJudger});
    auto LessDetailJudger = new JudgerType();
    LessDetailJudger->func = lessDetailJudger;
    LessDetailJudger->determined = true;
    LessDetailJudger->ruleset = {
        {0, {15, true, {-1}}},
        {1, {GREEN | BLUE | BRIGHT, false, {-1, 0}}}
    };
    judgers.insert({"less_detail", LessDetailJudger});
    auto CountOnlyJudger = new JudgerType();
    CountOnlyJudger->func = countOnlyJudger;
    CountOnlyJudger->determined = true;
    CountOnlyJudger->ruleset = {
        {0, {15, false, {}}},
        {1, {RED | BLUE, false, {}}}
    };
    judgers.insert({"count_only", CountOnlyJudger});
    validators.insert({"acceptDict", commonValidator});
    validators.insert({"empty", noValidator});
    problemsets.insert({"answerDict", commonProblemset});
    problemsets.insert({"acceptDict", hardcoreProblemset});
    gamemodes.insert({"common", {CommonJudger, commonValidator, commonProblemset, translate("{gamemode.common}")}});
    gamemodes.insert({"less_detail", {LessDetailJudger, commonValidator, commonProblemset, translate("{gamemode.less_detail}")}});
    gamemodes.insert({"count_only", {CountOnlyJudger, commonValidator, commonProblemset, translate("{gamemode.count_only}")}});
    gamemodes.insert({"no_validator", {CommonJudger, noValidator, commonProblemset, translate("{gamemode.no_validator}")}});
    gamemodes.insert({"hardcore_dictionary", {CommonJudger, commonValidator, hardcoreProblemset, translate("{gamemode.hardcore_dictionary}")}});
}

void Options::load_plugins() {
    AUTOLOG(Logger::Debug);

    if (!pluginEnabled) return;

    auto getFile = [](std::string pluginDirectory, std::string originalPath)->std::filesystem::path {
        if (options->pluginPrependLocation) {
            std::filesystem::path path = "plugins/" + pluginDirectory;
            path /= originalPath;
            return path;
        }
        else return originalPath;
    };

    for (const auto& [id, directory] : pluginList) {
        logger.write(Logger::Debug, "LOAD", "加载插件: " + id);
        std::filesystem::path manifest = "plugins";
        manifest = manifest / directory / options->pluginManifest;
         try {
            std::ifstream ifs(manifest);
            if (!ifs) {
                logger.write(Logger::Error, "LOAD", "无法打开插件清单: " + manifest.string());
                continue;
            }
            json config;
            ifs >> config;
            if (!config.is_object() || !config.contains("id") || !config["id"].is_string()) {
                logger.write(Logger::Error, "LOAD", "无效的插件清单: " + manifest.string());
                continue;
            }
            if (config["id"] != id) {
                logger.write(Logger::Error, "LOAD", "插件 ID 不匹配: " + manifest.string());
                continue;
            }
            if ((pluginFeatures & PLUGIN_LANGUAGES) && config.contains("languages") && config["languages"].is_object()) {
                std::string lang = "";
                if (config["languages"].contains(language)) lang = language;
                else if (language.find('.') != language.npos && config["languages"].contains(language.substr(0, language.find('.')))) lang = language.substr(0, language.find('.'));
                else if (language.find('_') != language.npos && config["languages"].contains(language.substr(0, language.find('_')))) lang = language.substr(0, language.find('_'));
                else if (config["languages"].contains("en_US.UTF-8")) lang = "en_US.UTF-8";
                else if (config["languages"].contains("en_US")) lang = "en_US";
                else if (config["languages"].contains("en")) lang = "en";
                else logger.write(Logger::Warn, "LOAD", "无法为插件 " + id + " 找到合适的本地化数据，正在跳过");
                if (lang != "") {
                    const auto& detail = config["languages"][lang];
                    if (detail.is_object() && detail.contains("type") && detail["type"].is_string()) {
                        std::string type = detail["type"];
                        if (type == "inline") {
                            if (detail.contains("content") && detail["content"].is_array()) {
                                for (const auto& item : detail["content"]) {
                                    if (!item.is_object() || !item.contains("id") || !item.contains("content") || !item["id"].is_string() || !item["content"].is_string()) continue;
                                    addTranslation(language, item["id"].get<std::string>(), item["content"].get<std::string>());
                                }
                            }
                        }
                        else if (type == "file") {
                            if (detail.contains("content") && detail["content"].is_string()) {
                                std::filesystem::path langFile = getFile(directory, detail["content"].get<std::string>());
                                loadLanguage(language, langFile);
                            }
                        }
                        else {
                            logger.write(Logger::Warn, "LOAD", "无法为插件 " + id + " 加载本地化数据，正在跳过");
                        }
                    }
                }
            }
            std::string title = (config.contains("name") && config["name"].is_string()) ? (translate(config["name"].get<std::string>()) + " (" + id + ")") : id;
            std::stringstream info;
            info << "ID: " << id << '\n';
            info << translate("{slot.manifest}: ") << manifest.lexically_normal().generic_u8string() << '\n';
            if (config.contains("name") && config["name"].is_string()) info << translate("{slot.name}: ") << translate(config["name"].get<std::string>()) << '\n';
            if (config.contains("version") && config["version"].is_string()) info << translate("{slot.version}: ") << translate(config["version"].get<std::string>()) << '\n';
            if (config.contains("author") && config["author"].is_string()) info << translate("{slot.author}: ") << translate(config["author"].get<std::string>()) << '\n';
            if (config.contains("url") && config["url"].is_string()) info << translate("{slot.url}: ") << translate(config["url"].get<std::string>()) << '\n';
            if (config.contains("description") && config["description"].is_string()) info << translate("{slot.description}: ") << translate(config["description"].get<std::string>()) << '\n';
            if (config.contains("license") && config["license"].is_string()) info << translate("{slot.license}: ") << translate(config["license"].get<std::string>()) << '\n';
            info << translate("{slot.features}:");
            bool incompatibleFeatures = false;
            if (config.contains("languages") && config["languages"].is_object()) {
                if (pluginFeatures & PLUGIN_LANGUAGES) info << translate("\n - {feature.languages}");
                else logger.write(Logger::Warn, "LOAD", "插件 " + id + " 使用了语言功能，但该功能未启用，正在跳过相关内容"), incompatibleFeatures = true;
            }
            if (config.contains("initScripts") && config["initScripts"].is_array()) {
                if (pluginScripting) {
                    info << translate("\n - {feature.init_scripts}");
                    for (const auto& script : config["initScripts"]) {
                        if (script.is_string()) {
                            std::ifstream ifs(getFile(directory, script.get<std::string>()));
                            if (!ifs) continue;
                            std::stringstream ss;
                            ss << ifs.rdbuf();
                            ifs.close();
                            RunScript(ss.str(), id); //
                        }
                    }
                }
                else logger.write(Logger::Warn, "LOAD", "插件 " + id + " 使用了初始化脚本功能，但该功能未启用，正在跳过相关内容"), incompatibleFeatures = true;
            }
            if (config.contains("dictionaries") && config["dictionaries"].is_array()) {
                if (pluginFeatures & PLUGIN_DICTIONARIES) {
                    info << translate("\n - {feature.dictionaries}");
                    for (const auto& dict : config["dictionaries"]) {
                        if (dict.is_string()) {
                            if (dictionaries.count(dict)) {
                                logger.write(Logger::Warn, "LOAD", "插件 " + id + " 正在重定义字典 " + dict.get<std::string>() + "，请注意兼容性问题");
                            }
                            else dictionaries.insert({dict, {}});
                        }
                    }
                }
                else logger.write(Logger::Warn, "LOAD", "插件 " + id + " 使用了自定义字典功能，但该功能未启用，正在跳过相关内容"), incompatibleFeatures = true;
            }
            if (config.contains("words") && config["words"].is_object()) {
                if (pluginFeatures & PLUGIN_WORDS) {
                    info << translate("\n - {feature.words}");
                    for (const auto&[dict, list] : config["words"].items()) {
                        if (!list.is_array()) continue;
                        if (!dictionaries.count(dict)) {
                            if (pluginFeatures & PLUGIN_DICTIONARIES) {
                                logger.write(Logger::Warn, "LOAD", "插件 " + id + " 使用了未定义的字典 " + dict + "，正在自动创建");
                                dictionaries.insert({dict, {}});
                            }
                            else {
                                logger.write(Logger::Warn, "LOAD", "插件 " + id + " 使用了不可用的字典 " + dict + "，正在跳过加载");
                                continue;
                            }
                        }
                        auto& S = dictionaries[dict];
                        for (const auto& word : list) {
                            if (word.is_string() && basicValidation(word)) {
                                S.insert(word);
                            }
                        }
                        if (dict == "answer") {
                            auto& S2 = dictionaries["accept"];
                            for (const auto& word : list) {
                                if (word.is_string() && basicValidation(word)) {
                                    S2.insert(word);
                                }
                            }
                        }
                    }
                }
                else logger.write(Logger::Warn, "LOAD", "插件 " + id + " 使用了自定义词汇功能，但该功能未启用，正在跳过相关内容"), incompatibleFeatures = true;
            }
            if (config.contains("gamemodes") && config["gamemodes"].is_object()) {
                if (pluginFeatures & PLUGIN_GAMEMODES) {
                    info << translate("\n - {feature.gamemodes}");
                    for (const auto&[name, obj] : config["gamemodes"].items()) {
                        if (gamemodes.count(name)) {
                            logger.write(Logger::Warn, "LOAD", "插件 " + id + " 正在重定义游戏模式 " + name + "，已跳过处理");
                            continue;
                        }
                        if (!obj.is_object()) continue;
                        if (!obj.contains("judger") || !obj["judger"].is_string() || !judgers.count(obj["judger"].get<std::string>())) continue;
                        if (!obj.contains("validator") || !obj["validator"].is_string() || !validators.count(obj["validator"].get<std::string>())) continue;
                        if (!obj.contains("problemset") || !obj["problemset"].is_string() || !problemsets.count(obj["problemset"].get<std::string>())) continue;
                        Gamemode gamemode;
                        gamemode.displayName = (obj.contains("name") && obj["name"].is_string()) ? translate(obj["name"].get<std::string>()) : name;
                        gamemode.judger = judgers[obj["judger"].get<std::string>()];
                        gamemode.validator = validators[obj["validator"].get<std::string>()];
                        gamemode.problemset = problemsets[obj["problemset"].get<std::string>()];
                        gamemodes.insert({name, gamemode});
                    }
                }
                else logger.write(Logger::Warn, "LOAD", "插件 " + id + " 使用了自定义游戏模式功能，但该功能未启用，正在跳过相关内容"), incompatibleFeatures = true;
            }
            if (config.contains("searchEngines") && config["searchEngines"].is_array()) {
                if (pluginFeatures & PLUGIN_SEARCH_ENGINES) {
                    info << translate("\n - {feature.search_engines}");
                    for (const auto& item : config["searchEngines"]) {
                        if (item.is_object() && item.contains("id") && item.contains("url") && item["id"].is_string() && item["url"].is_string()) {
                            std::string id = item["id"];
                            std::string url = item["url"];
                            if (dictSearchEngines.count(id)) {
                                logger.write(Logger::Warn, "LOAD", "插件 " + id + " 正在重定义搜索引擎 " + id + "，已跳过处理");
                            }
                            else dictSearchEngines.insert({id, url});
                        }
                    }
                }
                else logger.write(Logger::Warn, "LOAD", "插件 " + id + " 使用了自定义搜索引擎功能，但该功能未启用，正在跳过相关内容"), incompatibleFeatures = true;
            }
            if (config.contains("postScripts")) {
                if (pluginScripting) {
                    info << translate("\n - {feature.post_scripts}");
                    for (const auto& script : config["postScripts"]) {
                        if (script.is_string()) {
                            std::ifstream ifs(getFile(directory, script.get<std::string>()));
                            if (!ifs) continue;
                            std::stringstream ss;
                            ss << ifs.rdbuf();
                            ifs.close();
                            RunScript(ss.str(), id);
                        }
                    }
                }
                else logger.write(Logger::Warn, "LOAD", "插件 " + id + " 使用了后置脚本功能，但该功能未启用，正在跳过相关内容"), incompatibleFeatures = true;
            }
            if (incompatibleFeatures) info << translate("\n* {feature.incompatible}");
            info << translate("\n* {feature.script_defined_tip}");
            pluginInfo.push_back({title, info.str()});
         }
         catch (const std::exception& e) {
             logger.write(Logger::Error, "LOAD", "加载插件失败: " + id + " - " + e.what());
         }
    }
}

void Options::post_load() {
    for (const auto& [id, judger] : judgers) {
        if (judger->determined) {
            determinedJudgers.push_back({id, judger});
        }
    }

    for (const auto& [id, dict] : dictionaries) {
        fullDictionary.insert(dict.begin(), dict.end());
    }
}

Options* options;