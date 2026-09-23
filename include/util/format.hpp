#pragma once

#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace format_detail {
    inline const char* argument(const std::string& value) { return value.c_str(); }
    template<class Value>
    const Value& argument(const Value& value) {
        return value;
    }
} // namespace format_detail

template<class... Args>
std::string formatString(const std::string& pattern, const Args&... args) {
    const int length = std::snprintf(nullptr, 0, pattern.c_str(), format_detail::argument(args)...);
    if (length < 0) throw std::runtime_error("Cannot measure formatted string");
    std::vector<char> buffer(length + 1);
    const int written = std::snprintf(buffer.data(), buffer.size(), pattern.c_str(), format_detail::argument(args)...);
    if (written != length) throw std::runtime_error("Cannot render formatted string");
    return std::string(buffer.data(), length);
}
