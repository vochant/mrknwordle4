#include "dictReader.hpp"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include "io/stream_reader.h"
#include "nbt_tags.h"

namespace {
    bool valid_word(std::string_view word) {
        return word.size() == 5 && std::all_of(word.begin(), word.end(), [](unsigned char ch) {
            return ch >= 'a' && ch <= 'z';
        });
    }

    void add_word(std::vector<std::string>& words, const std::string& word) {
        if (!valid_word(word)) return;
        words.push_back(word);
    }

    std::vector<std::string> read_plain(std::istream& input) {
        std::vector<std::string> words;
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (!line.empty() && line.front() != '#') add_word(words, line);
        }
        if (input.bad()) throw std::runtime_error("Cannot read dictionary");
        return words;
    }

    std::vector<std::string> read_json(std::istream& input) {
        const auto document = nlohmann::json::parse(input);
        const auto& values = document.at("words");
        if (!values.is_array()) throw std::invalid_argument("Dictionary words must be an array");
        std::vector<std::string> words;
        for (const auto& value : values) add_word(words, value.get<std::string>());
        return words;
    }

    std::vector<std::string> read_nbt(std::istream& input) {
        nbt::io::stream_reader reader(input);
        if (reader.read_type() != nbt::tag_type::Compound) throw std::invalid_argument("Expected NBT compound");
        reader.read_string();
        nbt::tag_compound root;
        root.read_payload(reader);
        if (!root.has_key("words") || root["words"].get_type() != nbt::tag_type::List) {
            throw std::invalid_argument("Expected NBT words list");
        }
        const auto& values = root["words"].as<nbt::tag_list>();
        if (values.el_type() != nbt::tag_type::String) throw std::invalid_argument("Expected NBT string list");
        std::vector<std::string> words;
        for (const auto& value : values) add_word(words, value.as<nbt::tag_string>().get());
        return words;
    }

    class PlainDictReader : public DictReader {
        std::filesystem::path file;

    public:
        std::vector<std::string> read() override {
            std::ifstream input(file);
            if (!input) throw std::runtime_error("Cannot open dictionary: " + file.string());
            return read_plain(input);
        }

        explicit PlainDictReader(std::filesystem::path path) : file(std::move(path)) {}
    };

    class JSONDictReader : public DictReader {
        std::filesystem::path file;

    public:
        std::vector<std::string> read() override {
            std::ifstream input(file);
            if (!input) throw std::runtime_error("Cannot open dictionary: " + file.string());
            return read_json(input);
        }

        explicit JSONDictReader(std::filesystem::path path) : file(std::move(path)) {}
    };

    class NBTDictReader : public DictReader {
        std::filesystem::path file;

    public:
        std::vector<std::string> read() override {
            std::ifstream input(file, std::ios::binary);
            if (!input) throw std::runtime_error("Cannot open dictionary: " + file.string());
            return read_nbt(input);
        }

        explicit NBTDictReader(std::filesystem::path path) : file(std::move(path)) {}
    };
} // namespace

std::unique_ptr<DictReader> createDictReader(const std::filesystem::path& path, std::string format) {
    std::transform(format.begin(), format.end(), format.begin(), [](unsigned char ch) { return std::tolower(ch); });
    if (format == "plain") return std::make_unique<PlainDictReader>(path);
    if (format == "json") return std::make_unique<JSONDictReader>(path);
    if (format == "nbt") return std::make_unique<NBTDictReader>(path);
    throw std::invalid_argument("Unknown dictionary format: " + format);
}

std::vector<std::string> read_dict(const std::string& contents, const std::string& type) {
    std::istringstream input(contents);
    auto format = type;
    std::transform(format.begin(), format.end(), format.begin(), [](unsigned char ch) { return std::tolower(ch); });
    if (format == "plain") return read_plain(input);
    if (format == "json") return read_json(input);
    if (format == "nbt") return read_nbt(input);
    throw std::invalid_argument("Unknown dictionary format: " + type);
}
