#include "plugins/runtime.hpp"
#include "plugins/files.hpp"
#include <fstream>
#include <stdexcept>
#include <algorithm>

std::shared_ptr<Runtime> create_runtime(
    const std::string& backend, PluginFiles& files, const std::string& entry,
    const std::map<std::string, std::string>& modules
) {
    constexpr size_t maxModuleBytes = 1024 * 1024;
    const auto& source = files.read(entry, maxModuleBytes);
    if (backend != "lua" && !modules.empty()) {
        throw std::invalid_argument("Only Lua supports named source modules");
    }
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
    if (backend == "lua") return create_lua_runtime(source, sources);
#endif
    throw std::invalid_argument("Plugin backend not built: " + backend);
}
