#pragma once

#include <filesystem>
#include <memory>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

class Runtime {
public:
    virtual ~Runtime() = default;
    virtual void validateGrader(const std::string& name) = 0;
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

std::shared_ptr<Runtime> create_runtime(
    const std::string& backend, PluginFiles& files, const std::string& entry,
    const std::map<std::string, std::string>& modules
);

std::shared_ptr<Runtime> create_lua_runtime(
    const std::string& source,
    const std::map<std::string, std::string>& modules = {}
);
