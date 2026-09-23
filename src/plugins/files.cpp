#include "plugins/files.hpp"
#include <fstream>
#include <stdexcept>

namespace {
    void check_path(const std::filesystem::path& path) {
        const auto name = path.generic_string();
        if (name.empty() || path.is_absolute() ||
            name.find_first_of(":\\") != std::string::npos ||
            name.find('\0') != std::string::npos
        ) {
            throw std::invalid_argument("Plugin paths must use relative forward-slash paths");
        }
        for (const auto& component : path) {
            if (component == "..") {
                throw std::invalid_argument("Plugin paths cannot contain ..");
            }
        }
    }

    void check_within(const std::filesystem::path& file, const std::filesystem::path& root) {
        const auto relative = file.lexically_relative(root);
        if (relative.empty() || *relative.begin() == "..") {
            throw std::invalid_argument("Plugin path escapes its directory");
        }
    }
} // namespace

PluginFiles::PluginFiles(const std::filesystem::path& directory, bool topLevel) {
    check_path(directory);
    if (topLevel) {
        root = std::filesystem::canonical(directory);
        if (!std::filesystem::is_directory(root)) {
            throw std::invalid_argument("Plugin root must be a directory");
        }
        return;
    }
    auto plugins = std::filesystem::canonical("plugins");
    root = std::filesystem::canonical(plugins / directory);
    check_within(root, plugins);
    if (root == plugins || !std::filesystem::is_directory(root)) {
        throw std::invalid_argument("Plugin must have its own directory");
    }
}

const std::string& PluginFiles::read(const std::string& relative, size_t limit) {
    check_path(relative);
    auto path = std::filesystem::canonical(root / relative);
    check_within(path, root);
    if (!std::filesystem::is_regular_file(path)) {
        throw std::invalid_argument("Plugin resource must be a regular file: " + relative);
    }
    auto found = cache.find(path);
    if (found != cache.end()) {
        if (found->second.size() > limit) {
            throw std::invalid_argument("Plugin resource exceeds size limit: " + relative);
        }
        return found->second;
    }
    if (cache.size() >= 128) throw std::invalid_argument("Plugin exceeds 128 resource files");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open plugin resource: " + relative);
    std::string contents(limit + 1, '\0');
    input.read(contents.data(), contents.size());
    contents.resize(input.gcount());
    if (input.bad()) throw std::runtime_error("Cannot read plugin resource: " + relative);
    if (contents.size() > limit || totalBytes + contents.size() > 16 * 1024 * 1024) {
        throw std::invalid_argument("Plugin resource exceeds file or 16 MiB bundle limit: " + relative);
    }
    totalBytes += contents.size();
    return cache.emplace(path, std::move(contents)).first->second;
}
