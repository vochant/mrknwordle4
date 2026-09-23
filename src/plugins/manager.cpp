#include "i18n.hpp"
#include "plugins/manager.hpp"
#include "registry.hpp"
#include "json_schema.hpp"
#include "plugins/runtime.hpp"
#include "plugins/files.hpp"
#include "logger.hpp"
#include "dictReader.hpp"
#include <fstream>
#include <sstream>

namespace {
    bool valid_id(const std::string& id) {
        return !id.empty() && std::all_of(id.begin(), id.end(), [](unsigned char character) {
            return character >= 'a' && character <= 'z' ||
                character >= '0' && character <= '9' ||
                character == '.' || character == '_' || character == '-';
        });
    }

} // namespace

std::string PluginInfo::summary() const {
    std::string details;
    for (const auto& diagnostic : diagnostics) details += diagnostic + "\n";
    return tr(msg::PluginSummary {
        name, id, version, status == PluginStatus::Loaded ? tr(Msg::PluginLoaded) : tr(Msg::PluginFailed), author,
        license, url, description, details
    });
}

PluginManager::PluginManager(const Options& settings, Registry& resources) : settings(settings), res(resources) {}

void PluginManager::load(const PluginSource& source, const std::string& manifestName, bool topLevel, bool required) {
    PluginInfo info;
    info.id = source.id;
    info.name = source.id;
    logger.write(Logger::Debug, "LOAD", "加载插件: " + source.id);
    try {
        if (!valid_id(source.id)) throw std::invalid_argument("Invalid plugin id");
        PluginFiles files(source.directory, topLevel);
        auto mf = nlohmann::json::parse(files.read(manifestName));
        checkSchema(mf);
        check_fields(mf, {
            "schemaVersion", "id", "name", "version", "author",
            "homepage", "description", "license", "runtime",
            "dictionaries", "words", "graders"
        });
        if (mf.at("id").get<std::string>() != source.id) {
            throw std::invalid_argument("Manifest id does not match configured id");
        }
        info.name = safe_text(mf.at("name").get<std::string>());
        info.version = mf.at("version").get<std::string>();
        if (info.name.empty() || info.version.empty()) {
            throw std::invalid_argument("Plugin name and version cannot be empty");
        }
        info.author = mf.value("author", "");
        info.url = mf.value("homepage", "");
        info.description = safe_text(mf.value("description", std::string()));
        info.license = mf.value("license", "");
        auto cand = res;
        std::map<std::string, std::shared_ptr<Runtime>> runtimes;
        std::map<std::string, std::set<std::string>> runtimeExports;
        if (mf.contains("runtime")) {
            const auto& specs = mf.at("runtime");
            if (!specs.is_array() || specs.empty()) {
                throw std::invalid_argument("runtime must be a non-empty array");
            }
            for (const auto& spec : specs) {
                check_fields(spec, {"id", "backend", "entry", "modules", "exports"});
                auto id = spec.at("id").get<std::string>();
                auto backend = spec.at("backend").get<std::string>();
                if (!valid_id(id) || runtimes.count(id)) {
                    throw std::invalid_argument("Invalid or duplicate runtime id");
                }
                if (!settings.plugins.runtimes.count(backend)) {
                    throw std::invalid_argument("Plugin backend is not allowed: " + backend);
                }
                const auto& declared = spec.at("exports");
                if (!declared.is_array()) throw std::invalid_argument("runtime.exports must be an array");
                std::set<std::string> names;
                for (const auto& value : declared) {
                    auto name = value.get<std::string>();
                    if (name.empty() || !names.insert(name).second) {
                        throw std::invalid_argument("Invalid or duplicate runtime export");
                    }
                }
                auto modules = spec.value("modules", nlohmann::json::object());
                runtimes.emplace(id, create_runtime(
                    backend, files, spec.at("entry").get<std::string>(),
                    modules.get<std::map<std::string, std::string>>()
                ));
                runtimeExports.emplace(id, std::move(names));
            }
        }
        if (mf.contains("graders")) {
            const auto& graders = mf.at("graders");
            if (runtimes.empty() || !graders.is_object()) {
                throw std::invalid_argument("graders requires a runtime and an object");
            }
            for (const auto& entry : graders.items()) {
                if (!valid_id(entry.key()) || entry.key().find(source.id + ".") != 0 ||
                    cand.graders.count(entry.key())
                ) {
                    throw std::invalid_argument("Invalid or duplicate grader id: " + entry.key());
                }
                check_fields(entry.value(), {"name", "runtime", "exports", "deterministic", "states"});
                auto displayName = safe_text(entry.value().at("name").get<std::string>());
                if (displayName.empty()) throw std::invalid_argument("Grader name cannot be empty");
                auto backend = entry.value().at("runtime").get<std::string>();
                auto runtimeId = entry.value().at("runtime").get<std::string>();
                if (!runtimes.count(runtimeId)) throw std::invalid_argument("Unknown grader runtime");
                auto runtime = runtimes.at(runtimeId);
                const auto& exports = entry.value().at("exports");
                if (!exports.is_object() || !exports.contains("check")) {
                    throw std::invalid_argument("Grader exports must declare check");
                }
                std::set<std::string> exportNames;
                auto exportName = [&](const char* key, bool required) {
                    if (!exports.contains(key)) {
                        if (required) throw std::invalid_argument(std::string("Missing grader export: ") + key);
                        return std::string();
                    }
                    auto value = exports.at(key).get<std::string>();
                    bool declared = false;
                    declared = runtimeExports.at(runtimeId).count(value);
                    if (!declared) throw std::invalid_argument("Grader export is not declared by runtime");
                    if (value.empty() || !exportNames.insert(value).second) {
                        throw std::invalid_argument("Grader exports must be distinct non-empty names");
                    }
                    return value;
                };
                auto checkName = exportName("check", true);
                auto compatibleName = exportName("compatible", false);
                auto startName = exportName("start", false);
                auto finishName = exportName("finish", false);
                runtime->validateGrader(checkName);
                if (!compatibleName.empty()) runtime->validateCompatible(compatibleName);
                if (!startName.empty()) runtime->validateLifecycle(startName);
                if (!finishName.empty()) runtime->validateLifecycle(finishName);
                auto grader = std::make_shared<GraderType>();
                grader->name = displayName;
                grader->deterministic = entry.value().value("deterministic", false);
                const auto& states = entry.value().at("states");
                if (!states.is_object() || states.empty() || states.size() > 256) {
                    throw std::invalid_argument("Grader states must be a nonempty object with at most 256 states");
                }
                for (const auto& state : states.items()) {
                    size_t parsed = 0;
                    int id;
                    try {
                        id = std::stoi(state.key(), &parsed);
                    }
                    catch (...) {
                        throw std::invalid_argument("Invalid grader state ID: " + state.key());
                    }
                    if (parsed != state.key().size() || id < 0 || id > 255 || std::to_string(id) != state.key()) {
                        throw std::invalid_argument("Grader state ID must be a canonical integer from 0 to 255");
                    }
                    check_fields(state.value(), {"color", "mode", "overrides"});
                    auto mode = state.value().at("mode").get<std::string>();
                    if (mode != "mark" && mode != "spoiler") {
                        throw std::invalid_argument("Grader state mode must be mark or spoiler");
                    }
                    auto overrides = state.value().at("overrides").get<std::set<int>>();
                    if (std::any_of(overrides.begin(), overrides.end(), [](int other) {
                        return other < -1 || other > 255;
                    })) {
                        throw std::invalid_argument("Grader state override must be -1 or a state ID");
                    }
                    grader->ruleset.emplace(id, GraderType::GraderResult {
                        parse_color(state.value().at("color").get<std::string>()), mode == "spoiler",
                        std::move(overrides)
                    });
                }
                for (const auto&[id, state] : grader->ruleset) {
                    for (int other : state.overrides) {
                        if (other != -1 && !grader->ruleset.count(other)) {
                            throw std::invalid_argument(
                                "Grader state overrides an undeclared state: " + std::to_string(other)
                            );
                        }
                    }
                }
                grader->func = [runtime, checkName](const std::string& guess, const std::string& answer) {
                    return runtime->grade(checkName, guess, answer);
                };
                grader->start = startName.empty() ?
                    GraderLifecycleFunc {} :
                    [runtime, startName] { runtime->start(startName); };
                grader->finish = finishName.empty() ?
                    GraderLifecycleFunc {} :
                    [runtime, finishName]() { runtime->finish(finishName); };
                grader->hasCompatible = !compatibleName.empty();
                grader->compatible = [runtime, compatibleName](
                    const std::string& guess, const std::string& answer,
                    const std::vector<int>& feedback
                ) {
                    return runtime->compatible(compatibleName, guess, answer, feedback);
                };
                cand.graders.emplace(entry.key(), std::move(grader));
            }
        }
        if (mf.contains("dictionaries")) {
            const auto& dicts = mf.at("dictionaries");
            if (!dicts.is_object()) throw std::invalid_argument("dictionaries must be an object");
            for (const auto& e : dicts.items()) {
                const auto& id = e.key();
                if (!valid_id(id) || id.find(source.id + ".") != 0 || cand.dicts.count(id)) {
                    throw std::invalid_argument("Invalid or duplicate dictionary id: " + id);
                }
                auto dict = e.value();
                dict["name"] = safe_text(dict.at("name").get<std::string>());
                cand.dicts.emplace(id, parse_dict(dict));
            }
        }
        if (mf.contains("words")) {
            const auto& words = mf.at("words");
            if (!words.is_array()) throw std::invalid_argument("words must be an array");
            for (const auto& entry : words) {
                check_fields(entry, {"filter", "target", "file", "type"});
                WordRule rule;
                rule.filter = entry.at("filter").get<std::string>();
                rule.target = entry.at("target").get<std::string>();
                auto type = entry.at("type").get<std::string>();
                if (rule.filter != "*" && (!valid_id(rule.filter) || !cand.dicts.count(rule.filter))) {
                    throw std::invalid_argument("Unknown word filter: " + rule.filter);
                }
                if (rule.target != "accept" && rule.target != "answer") {
                    throw std::invalid_argument("Word target must be accept or answer");
                }
                const auto& contents = files.read(entry.at("file").get<std::string>());
                const auto loaded = read_dict(contents, type);
                rule.words.insert(loaded.begin(), loaded.end());
                cand.words.push_back(std::move(rule));
            }
        }
        cand.applyWords();
        cand.rebuildIndexes();
        res = std::move(cand);
        logger.write(Logger::Info, "LOAD", "已加载插件: " + info.id + " " + info.version);
    }
    catch (const std::exception& error) {
        if (required) {
            logger.write(Logger::Error, "LOAD", "加载必需插件失败: " + source.id + " - " + error.what());
            throw std::runtime_error("Cannot load required plugin " + source.id + ": " + error.what());
        }
        logger.write(Logger::Warn, "LOAD", "加载插件失败: " + source.id + " - " + error.what());
        info.status = PluginStatus::Failed;
        info.diagnostics.push_back(error.what());
    }
    plugins.push_back(std::move(info));
}

void PluginManager::load() {
    if (loaded) return;
    loaded = true;
    load(PluginSource {"core", "."}, "core.json", true, true);
    if (!settings.plugins.enabled) return;
    for (const auto& source : settings.plugins.sources) load(source);
}

PluginManager* pluginManager = nullptr;
