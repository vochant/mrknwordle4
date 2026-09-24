#pragma once

#include "game_types.hpp"
#include "plugins/registration.hpp"
#include <functional>
#include <filesystem>
#include <memory>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

class Runtime {
public:
    virtual ~Runtime() = default;
    virtual ActivePluginResources takeRegistrations() { return {}; }
    virtual void validateGrader(const std::string& name) = 0;
    virtual GraderFunc bindGrader(const std::string& name) {
        validateGrader(name);
        return [this, name](const std::string& guess, const std::string& answer) {
            return grade(name, guess, answer);
        };
    }
    virtual void validateCompatible(const std::string&) {
        throw std::invalid_argument("Runtime does not support compatible exports");
    }
    virtual void validateLifecycle(const std::string&) {
        throw std::invalid_argument("Runtime does not support lifecycle exports");
    }
    virtual std::vector<int> grade(const std::string& name, const std::string& guess, const std::string& answer) = 0;
    virtual void start(const std::string&) { throw std::logic_error("Runtime does not support lifecycle exports"); }
    virtual void finish(const std::string&) { throw std::logic_error("Runtime does not support lifecycle exports"); }
    virtual bool compatible(const std::string&, const std::string&, const std::string&, const std::vector<int>&) {
        throw std::logic_error("Runtime does not support compatible exports");
    }
};

class PluginFiles;

struct LuaRuntimeOptions {
    std::string pluginId;
    std::function<std::string(const std::string&)> readFile;
    std::set<std::string> readableFiles;
    std::set<std::string> registerGraders;
    std::set<std::string> registerDictionaries;
    std::set<std::string> registerWords;
    bool allowLog = false;
};

std::shared_ptr<Runtime> create_runtime(
    const std::string& backend, PluginFiles& files, const std::string& entry,
    const std::map<std::string, std::string>& modules, const LuaRuntimeOptions& options = {}
);

std::shared_ptr<Runtime> create_native_runtime();

std::shared_ptr<Runtime> create_lua_runtime(
    const std::string& source,
    const std::map<std::string, std::string>& modules = {}, const LuaRuntimeOptions& options = {}
);
