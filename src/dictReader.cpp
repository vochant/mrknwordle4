#include "dictReader.hpp"

#include <fstream>
#include <cctype>
#include <nlohmann/json.hpp>
#include <exception>
#include "io/stream_reader.h"
#include "nbt_tags.h"

using namespace nlohmann;
using namespace nbt;

bool DictReader::validate(std::string str) {
    if (str.length() != 5) return false;
    for (int i = 0; i < 5; i++) if (str[i] < 'a' || str[i] > 'z') return false;
    return true;
}

class PlainDictReader : public DictReader {
private:
    std::filesystem::path file;

public:
    std::vector<std::string> read() override {
        std::ifstream ifs(file);
        if (!ifs) return {};
        std::vector<std::string> res;
        std::string word;
        while (ifs >> word && word.length()) {
            if (validate(word)) res.push_back(word);
        }
        return res;
    }

    PlainDictReader(const std::filesystem::path& path) : file(path) {}
};

class JSONDictReader : public DictReader {
private:
    std::filesystem::path file;

public:
    std::vector<std::string> read() override {
        std::ifstream ifs(file);
        if (!ifs) return {};
        try {
            std::vector<std::string> res;
            json obj;
            ifs >> obj;
            if (!obj.is_object() || !obj.contains("words") || !obj["words"].is_array()) return {};
            for (const auto& item : obj["words"]) {
                if (item.is_string()) {
                    auto word = item.get<std::string>();
                    if (validate(word)) res.push_back(word);
                }
            }
            return res;
        }
        catch (const std::exception& e) {
            return {};
        }
    }

    JSONDictReader(const std::filesystem::path& path) : file(path) {}
};

class NBTDictReader : public DictReader {
private:
    std::filesystem::path file;

public:
    std::vector<std::string> read() override {
        std::ifstream ifs(file, std::ios::binary);
        if (!ifs) return {};
        try {
            io::stream_reader reader(ifs);
            if (reader.read_type() != tag_type::Compound) return {};
            reader.read_string();
            tag_compound root;
            root.read_payload(reader);
            if (!root.has_key("words") || root["words"].get_type() != tag_type::List) return {};
            std::vector<std::string> res;
            const tag_list& words = root["words"].as<tag_list>();
            if (words.el_type() != tag_type::String) return {};
            for (const auto& item : words) {
                auto word = item.as<tag_string>().get();
                if (validate(word)) res.push_back(word);
            }
            return res;
        }
        catch (const std::exception& e) {
            return {};
        }
    }

    NBTDictReader(const std::filesystem::path& path) : file(path) {}
};

class InvalidDictReader : public DictReader {
public:
    std::vector<std::string> read() override {
        return {};
    }
};

std::shared_ptr<DictReader> createDictReader(const std::filesystem::path& path, std::string type) {
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) { return std::tolower(c); });
    if (type == "plain") return std::make_shared<PlainDictReader>(path);
    else if (type == "json") return std::make_shared<JSONDictReader>(path);
    else if (type == "nbt") return std::make_shared<NBTDictReader>(path);
    else return std::make_shared<InvalidDictReader>();
}