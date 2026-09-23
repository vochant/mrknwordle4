#pragma once

#include "options.hpp"
#include <memory>

class Registry;

enum class PluginStatus { Loaded, Failed };

struct PluginInfo {
    std::string id, name, version, author, url, description, license;
    PluginStatus status = PluginStatus::Loaded;
    std::vector<std::string> diagnostics;
    std::string summary() const;
};

class PluginManager {
    const Options& settings;
    Registry& res;
    std::vector<PluginInfo> plugins;
    bool loaded = false;

    void load(
        const PluginSource& source, const std::string& manifest = "manifest.json", bool topLevel = false,
        bool required = false
    );

public:
    PluginManager(const Options& settings, Registry& resources);
    void load();
    const std::vector<PluginInfo>& entries() const { return plugins; }
};

extern PluginManager* pluginManager;
