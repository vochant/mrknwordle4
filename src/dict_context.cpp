#include "options.hpp"
#include "registry.hpp"
#include "dict_context.hpp"
#include "util/dialog.hpp"
#include "eventbus.hpp"
#include "i18n.hpp"
#include "logger.hpp"

DictContext::DictContext(
    std::string parent, std::shared_ptr<GraderType> grader,
    bool answerOnly, bool validate, bool showImpossible
) : parent(std::move(parent)), validate(validate), showImpossible(showImpossible), grader(std::move(grader)) {
    if (!this->grader || (!this->grader->deterministic && !this->grader->hasCompatible)) {
        throw std::invalid_argument("Dictionary filtering requires a deterministic grader or compatible checker");
    }
    const auto& dict = registry->dicts.at(this->parent);
    int ix = 0;
    for (const auto& word : answerOnly ? dict.answers : dict.acceptable) {
        all.emplace(word, std::make_pair(++ix, registry->wordColor(this->parent, word)));
    }
    remaining = all;
}

void DictContext::add(const std::string word) {
    Restriction res;
    res.displayWord = word;
    res.word = registry->dicts.at(parent).normalize(word);
    if (!registry->dicts.at(parent).valid(res.word)) {
        throw std::invalid_argument("Invalid restriction word");
    }
    for (int i = 0; i < 5; i++) res.state[i] = grader->ruleset.begin();
    if (validate && !registry->dicts.at(parent).acceptable.count(res.word)) {
        throw std::invalid_argument("Restriction word is not accepted by this dictionary");
    }
    restrictions.push_back(res);
}

void DictContext::next(int wordIx, int charIx) {
    if (wordIx < 0 || wordIx >= restrictions.size()) return;
    if (charIx < 0 || charIx >= 5) return;
    restrictions[wordIx].state[charIx]++;
    if (restrictions[wordIx].state[charIx] == grader->ruleset.end()) {
        restrictions[wordIx].state[charIx] = grader->ruleset.begin();
    }
}

void DictContext::remove(int index) {
    if (index < 0 || index >= restrictions.size()) return;
    restrictions.erase(restrictions.begin() + index);
}

int DictContext::count() { return restrictions.size(); }

bool DictContext::apply() {
    remaining.clear();
    for (const auto& [word, conf] : all) {
        bool possible = true;
        for (const auto& res : restrictions) {
            try {
                std::vector<int> fb(5);
                for (int i = 0; i < 5; i++) fb[i] = res.state[i]->first;
                if (grader->deterministic) possible = grader->func(res.word, word) == fb;
                else possible = grader->compatible(res.word, word, fb);
            }
            catch (const std::exception& e) {
                logger.write(Logger::Error, "JUDGE",
                    "评测时出错：" + word + " <=> " + res.word + " - " + e.what()
                );
                confirm(64, 8, tr(msg::ErrorGrader {e.what()}), []() {
                    evbus->send(Events::Shutdown {});
                });
                return true;
            }
            if (!possible) break;
        }
        if (possible) remaining.insert({word, conf});
        else if (showImpossible) remaining.insert({word, {conf.first, options->colors.dictionaryImpossible}});
    }
    return false;
}

DictContext* g_dict_ctxt;
