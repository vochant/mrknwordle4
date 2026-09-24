#include "plugins/runtime.hpp"
#include "plugins/files.hpp"
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <map>

namespace {
    std::vector<int> wordle_grader(const std::string& guess, const std::string& answer) {
        std::vector<int> result(5, 0);
        bool ignore[5] {};
        short appears[26] {};
        for (short i = 0; i < 5; i++) ignore[i] = false;
        for (short i = 0; i < 26; i++) appears[i] = 0;
        for (short i = 0; i < 5; i++) {
            if (guess[i] == answer[i]) {
                result[i] = 2;
                ignore[i] = true;
            }
            else appears[answer[i] - 'a']++;
        }
        for (short i = 0; i < 5; i++) {
            if (appears[guess[i] - 'a'] && !ignore[i]) {
                result[i] = 1;
                appears[guess[i] - 'a']--;
            }
        }
        return result;
    }

    std::vector<int> letter_presence_grader(const std::string& guess, const std::string& answer) {
        std::vector<int> result(5, 0);
        short appears[26] {};
        for (short i = 0; i < 26; i++) appears[i] = 0;
        for (short i = 0; i < 5; i++) appears[answer[i] - 'a']++;
        for (short i = 0; i < 5; i++) {
            if (appears[guess[i] - 'a']) {
                result[i] = 1;
                appears[guess[i] - 'a']--;
            }
        }
        return result;
    }

    std::vector<int> match_count_grader(const std::string& guess, const std::string& answer) {
        const auto wordle = letter_presence_grader(guess, answer);
        std::vector<int> result(5, 0);
        const auto cnt = std::count(wordle.begin(), wordle.end(), 1);
        std::fill_n(result.begin(), cnt, 1);
        return result;
    }

    std::vector<int> hardle_grader(const std::string& guess, const std::string& answer) {
        const auto wordle = wordle_grader(guess, answer);
        std::vector<int> result(5, 0);
        const auto green = std::count(wordle.begin(), wordle.end(), 2);
        const auto yellow = std::count(wordle.begin(), wordle.end(), 1);
        std::fill_n(result.begin(), green, 2);
        std::fill_n(result.begin() + green, yellow, 1);
        return result;
    }

    class NativeRuntime : public Runtime {
        using Grader = std::vector<int> (*)(const std::string&, const std::string&);
        const std::map<std::string, Grader> graders {
            {"wordle", wordle_grader},
            {"letter_presence", letter_presence_grader},
            {"match_count", match_count_grader},
            {"hardle", hardle_grader}
        };

    public:
        void validateGrader(const std::string& name) override {
            if (!graders.count(name)) throw std::invalid_argument("Unknown native grader: " + name);
        }

        GraderFunc bindGrader(const std::string& name) override {
            validateGrader(name);
            return graders.at(name);
        }

        std::vector<int> grade(const std::string& name, const std::string& guess, const std::string& answer) override {
            return graders.at(name)(guess, answer);
        }
    };
}

std::shared_ptr<Runtime> create_runtime(
    const std::string& backend, PluginFiles& files, const std::string& entry,
    const std::map<std::string, std::string>& modules, const LuaRuntimeOptions& options
) {
    if (backend == "native") {
        if (!modules.empty()) throw std::invalid_argument("Native runtime does not support named source modules");
        return create_native_runtime();
    }
    constexpr size_t maxModuleBytes = 1024 * 1024;
    const auto& source = files.read(entry, maxModuleBytes);
    if (backend != "lua" && !modules.empty()) {
        throw std::invalid_argument("Only Lua supports named source modules");
    }
#ifndef WORDLE_HAS_LUA
    (void) options;
#endif
    std::map<std::string, std::string> sources;
    size_t total = source.size();
    for (const auto& [name, path] : modules) {
        if (name.empty() || name.size() > 128 || !std::all_of(name.begin(), name.end(), [](unsigned char letter) {
            return letter >= 'a' && letter <= 'z' ||
                letter >= 'A' && letter <= 'Z' ||
                letter >= '0' && letter <= '9' ||
                letter == '_' || letter == '.';
        })) {
            throw std::invalid_argument("Invalid Lua module name: " + name);
        }
        const auto& contents = files.read(path, maxModuleBytes);
        total += contents.size();
        if (total > maxModuleBytes) throw std::invalid_argument("Plugin code exceeds 1 MiB total");
        sources.emplace(name, contents);
    }
#ifdef WORDLE_HAS_LUA
    if (backend == "lua") return create_lua_runtime(source, sources, options);
#endif
    throw std::invalid_argument("Plugin backend not built: " + backend);
}

std::shared_ptr<Runtime> create_native_runtime() {
    return std::make_shared<NativeRuntime>();
}
