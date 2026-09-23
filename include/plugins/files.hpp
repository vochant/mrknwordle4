#pragma once

#include <filesystem>
#include <map>
#include <string>

class PluginFiles {
    std::filesystem::path root;
    std::map<std::filesystem::path, std::string> cache;
    size_t totalBytes = 0;

public:
    explicit PluginFiles(const std::filesystem::path& directory, bool topLevel = false);
    const std::string& read(const std::string& relative, size_t limit = 4 * 1024 * 1024);
};
