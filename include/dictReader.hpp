#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <memory>

class DictReader {
public:
    virtual ~DictReader() = default;
    virtual std::vector<std::string> read() = 0;
};

std::unique_ptr<DictReader> createDictReader(const std::filesystem::path& path, std::string type);
std::vector<std::string> read_dict(const std::string& contents, const std::string& type);
