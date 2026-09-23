#pragma once

#include "game_types.hpp"

struct Restriction {
    std::string word, displayWord;
    std::map<int, GraderType::GraderResult>::iterator state[5];
};

class DictContext {
public:
    std::string parent;
    bool validate, showImpossible;
    std::shared_ptr<GraderType> grader;
    std::vector<Restriction> restrictions;
    std::map<std::string, std::pair<int, Color>> all, remaining; // -1 = impossible

public:
    DictContext(
        std::string parent, std::shared_ptr<GraderType> grader, bool answerOnly, bool validate, bool showImpossible
    );
    void add(const std::string word);
    void next(int wordIndex, int charIndex);
    void remove(int index);
    int count();
    bool apply();
};

extern DictContext* g_dict_ctxt;
