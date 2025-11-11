#include "dict_context.hpp"
#include "util/dialog.hpp"
#include "ipc.hpp"
#include "i18n.hpp"
#include "logger.hpp"

DictContext::DictContext(std::string parent, JudgerType* judger) : parent(std::move(parent)), judger(judger) {
    int index = 0;
    for (const auto& word : options->dictionaries[this->parent]) {
        char color = 15;
        for (const auto&[category, icolor] : options->dictHighlights) {
            if (category == "*" || options->dictionaries.count(category) && options->dictionaries[category].count(word)) {
                color = icolor;
                break;
            }
        }
        all.insert({word, {++index, color}});
    }
    remaining = all;
}

void DictContext::addRestriction(const std::string word) {
    Restriction restriction;
    restriction.word = word;
    for (int i = 0; i < 5; i++) restriction.state[i] = judger->ruleset.begin();
    restrictions.push_back(restriction);
}

void DictContext::increaseRestriction(int wordIndex, int charIndex) {
    if (wordIndex < 0 || wordIndex >= restrictions.size()) return;
    if (charIndex < 0 || charIndex >= 5) return;
    restrictions[wordIndex].state[charIndex]++;
    if (restrictions[wordIndex].state[charIndex] == judger->ruleset.end()) {
        restrictions[wordIndex].state[charIndex] = judger->ruleset.begin();
    }
}

void DictContext::removeRestriction(int index) {
    if (index < 0 || index >= restrictions.size()) return;
    restrictions.erase(restrictions.begin() + index);
}

int DictContext::countRestrictions() {
    return restrictions.size();
}

bool DictContext::applyRestrictions() {
    remaining.clear();
    for (const auto& [word, conf] : all) {
        bool possible = true;
        for (const auto& restriction : restrictions) {
            try {
                auto result = judger->func(restriction.word, word);
                for (int i = 0; i < 5; i++) {
                    if (result[i] != restriction.state[i]->first) {
                        possible = false;
                        break;
                    }
                }
            }
            catch (const std::exception& e) {
                logger.write(Logger::Error, "JUDGE", "评测时出错：" + word + " <=> " + restriction.word + " - " + e.what());
                confirm(64, 8, translate("{hint.judge_failed}\n") + e.what(), []() {
                    ipc->send({"shutdown", 0});
                });
                return true;
            }
            if (!possible) break;
        }
        if (possible) remaining.insert({word, conf});
        else if (options->dictShowImpossible) remaining.insert({word, {conf.first, options->dictImpossibleColor}});
    }
    return false;
}

DictContext* g_dict_ctxt;