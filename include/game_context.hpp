#pragma once

#include "options.hpp"

struct HistoryType {
    std::string input;
    char colors[5];
};

enum class Result : char {
    INVALID, FAILED, INCORRECT, CORRECT
};

class GameContext {
public:
    std::string answer, id;
    std::vector<HistoryType> history;
    int state[26];
    char color[26];
    Gamemode* gamemode;

public:
    Result accept(std::string input);
    GameContext(Gamemode* gamemode, std::string id);
};

extern GameContext* g_context;