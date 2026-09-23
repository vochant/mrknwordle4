#pragma once

#include "game_types.hpp"

struct HistoryType {
    std::string input;
    Color colors[5];
};

enum class Result : char { INVALID, FAILED, INCORRECT, CORRECT };

class GameContext {
public:
    std::string answer, dictId, graderId;
    std::vector<HistoryType> history;
    int state[26];
    Color color[26];
    const GraderType* grader;
    const Dictionary* dict;
    bool validate, showAlphabet;
    int maxGuesses;
    ~GameContext();

public:
    Result accept(std::string input);
    GameContext(
        const GraderType& grader, const Dictionary& dictionary, std::string dictionaryId, std::string graderId,
        bool validate, bool answerOnly, bool showAlphabet, int maxGuesses
    );
};

extern GameContext* g_context;
