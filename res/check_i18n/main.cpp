#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

namespace fs = std::filesystem;
using nlohmann::json;

namespace {
    json read_json(const fs::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open language resource: " + path.string());
        json result;
        input >> result;
        return result;
    }

    std::set<std::string> message_keys(const json& document) {
        std::set<std::string> result;
        for (const auto& item : document.at("messages").items()) result.insert(item.key());
        return result;
    }

    void copy_files(
        const fs::path& source, const fs::path& output,
        const json& index, const std::vector<std::string>& locales
    ) {
        fs::path tmp = output;
        tmp += ".tmp";
        std::error_code error;
        fs::remove_all(tmp, error);
        if (error) throw std::runtime_error("Cannot clear temporary output: " + error.message());
        fs::create_directories(tmp, error);
        if (error) throw std::runtime_error("Cannot create temporary output: " + error.message());

        for (const auto& e : fs::recursive_directory_iterator(source)) {
            if (!e.is_regular_file() || e.path().extension() == ".json") continue;
            const auto target = tmp / fs::relative(e.path(), source);
            fs::create_directories(target.parent_path(), error);
            if (error) throw std::runtime_error("Cannot create output directory: " + error.message());
            fs::copy_file(e.path(), target, fs::copy_options::overwrite_existing, error);
            if (error) throw std::runtime_error("Cannot copy language resource: " + error.message());
        }
        for (const auto& locale : locales) {
            fs::copy_file(
                source / (locale + ".json"),
                tmp / (locale + ".json"),
                fs::copy_options::overwrite_existing, error
            );
            if (error) throw std::runtime_error("Cannot copy catalog " + locale + ": " + error.message());
        }
        std::ofstream mf(tmp / "locales.json", std::ios::binary);
        if (!mf) throw std::runtime_error("Cannot write filtered locale index");
        mf << index.dump(2) << '\n';
        if (!mf) throw std::runtime_error("Cannot write filtered locale index");

        fs::remove_all(output, error);
        if (error) throw std::runtime_error("Cannot replace i18n output: " + error.message());
        fs::rename(tmp, output, error);
        if (error) throw std::runtime_error("Cannot install i18n output: " + error.message());
    }
} // namespace

int main(int argc, char** argv) try {
    if (argc != 3) {
        std::cerr << "Usage: check_i18n <source-directory> <output-directory>\n";
        return 2;
    }
    const fs::path source = argv[1];
    const fs::path output = argv[2];
    if (fs::absolute(source) == fs::absolute(output)) {
        throw std::invalid_argument("i18n source and output directories must differ");
    }
    json index = read_json(source / "locales.json");
    const auto base = index.at("base").get<std::string>();
    const auto requested = index.at("locales").get<std::vector<std::string>>();
    const auto baseKeys = message_keys(read_json(source / (base + ".json")));

    std::vector<std::string> selected;
    for (const auto& language : requested) {
        try {
            const auto keys = message_keys(read_json(source / (language + ".json")));
            if (keys != baseKeys) throw std::invalid_argument("message keys do not match " + base);
            selected.push_back(language);
        }
        catch (const std::exception& error) {
            std::cerr << language << ": " << error.what() << '\n';
        }
    }
    if (selected.empty()) throw std::logic_error("No valid i18n catalogs");

    index["locales"] = selected;
    auto& aliases = index.at("aliases");
    for (auto it = aliases.begin(); it != aliases.end();) {
        if (std::find(selected.begin(), selected.end(), it.value().get<std::string>()) == selected.end()) {
            it = aliases.erase(it);
        }
        else it++;
    }
    copy_files(source, output, index, selected);
    return 0;
}
catch (const std::exception& error) {
    std::cerr << "i18n check failed: " << error.what() << '\n';
    return 1;
}
