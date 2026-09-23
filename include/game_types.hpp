#pragma once

#include "color.hpp"
#include "dictionary.hpp"
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

using GraderFunc = std::function<std::vector<int>(const std::string&, const std::string&)>;
using GraderLifecycleFunc = std::function<void()>;
using GraderCompatibleFunc = std::function<bool(const std::string&, const std::string&, const std::vector<int>&)>;

struct GraderType {
    struct GraderResult {
        Color color;
        bool isSpoiler;
        std::set<int> overrides;
    };
    bool deterministic = false;
    std::string name;
    GraderFunc func;
    GraderLifecycleFunc start, finish;
    GraderCompatibleFunc compatible;
    bool hasCompatible = false;
    std::map<int, GraderResult> ruleset;
};
