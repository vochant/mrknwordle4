#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <memory>

class DictReader {
protected:
    bool validate(std::string str);
 
public:
    virtual std::vector<std::string> read() = 0;
};

std::shared_ptr<DictReader> createDictReader(const std::filesystem::path& path, std::string type);