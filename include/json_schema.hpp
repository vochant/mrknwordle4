#pragma once

#include <nlohmann/json.hpp>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <algorithm>

inline void check_fields(const nlohmann::json& object, std::initializer_list<std::string> fields) {
    if (!object.is_object()) throw std::invalid_argument("Expected an object");
    for (const auto& entry : object.items()) {
        if (std::find(fields.begin(), fields.end(), entry.key()) == fields.end()) {
            throw std::invalid_argument("Unknown field: " + entry.key());
        }
    }
}

inline void checkSchema(const nlohmann::json& document) {
    if (!document.contains("schemaVersion") ||
        !document.at("schemaVersion").is_number_integer() ||
        document.at("schemaVersion") != 2
    ) {
        throw std::invalid_argument("Only schemaVersion 2 is supported");
    }
}
