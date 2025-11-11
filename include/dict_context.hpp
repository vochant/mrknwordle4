#pragma once

#include "options.hpp"

struct Restriction {
    std::string word;
    std::map<int, JudgerType::JudgerResult>::iterator state[5];
};

class DictContext {
public:
    std::string parent;
    JudgerType* judger;
    std::vector<Restriction> restrictions;
    std::map<std::string, std::pair<int, char>> all, remaining; // -1 = impossible

public:
    DictContext(std::string parent, JudgerType* judger);
    void addRestriction(const std::string word);
    void increaseRestriction(int wordIndex, int charIndex);
    void removeRestriction(int index);
    int countRestrictions();
    bool applyRestrictions();
};

extern DictContext* g_dict_ctxt;