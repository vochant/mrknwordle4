#include "registry.hpp"
#include "ui/screens.hpp"
#include "util/dialog.hpp"

#include "dict_context.hpp"
#include "eventbus.hpp"
#include "widgets/longmenu.hpp"
#include "widgets/menu.hpp"
#include "widgets/text.hpp"
#include "widgets/input.hpp"
#include "widgets/layout.hpp"
#include "widgets/link.hpp"
#include "widgets/button.hpp"
#include "i18n.hpp"
#include "options.hpp"
#include "logger.hpp"
#include <cstdio>
#include "platform.hpp"
#include "userdb.hpp"

#include <sstream>
#include <cctype>

namespace {
    std::string subst_search(std::string pattern, const std::string& value) {
        auto pos = pattern.find("%s");
        if (pos != std::string::npos) pattern.replace(pos, 2, value);
        return pattern;
    }

    std::string selection;
    bool isFilter = false;
} // namespace

std::unique_ptr<Layout> DictMenu() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries;
    entries.push_back({tr(msg::EntryBack {"Esc"}), []() {
        if (options->dict.cleanup) {
            delete g_dict_ctxt;
            g_dict_ctxt = nullptr;
        }
        evbus->send(Events::Main {});
    }, Key::Escape});
    entries.push_back({tr(msg::EntryView {"Enter"}), []() {
        evbus->send(Events::Route {"dictMain"});
    }, (int) '\r'});
    entries.push_back({tr(msg::EntryFilter {"F"}), []() {
        isFilter = true;
        evbus->send(Events::Route {"dictFilter"});
    }, (int) 'f'});
    entries.push_back({tr(msg::EntryCleanup {"C"}), []() {
        delete g_dict_ctxt;
        g_dict_ctxt = nullptr;
        evbus->send(Events::Route {"dictInit1"});
    }, (int) 'c'});
    auto menu = std::make_unique<Menu>(0, 0, 77, entries);
    layout->add(std::move(menu));
    return layout;
}

std::unique_ptr<Layout> DictMain() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"dictEntry"});
    }));
    auto getterFunc = [](int page) -> std::vector<std::string> {
        if (!g_dict_ctxt) return {};
        if (page < 0 || page * 19 >= g_dict_ctxt->remaining.size()) return {};
        std::vector<std::string> result;
        auto it = g_dict_ctxt->remaining.begin();
        std::advance(it, page * 19);
        for (int i = 0; i < 19 && it != g_dict_ctxt->remaining.end(); i++, it++) {
            std::string str = options->dict.showId ? ("[" + std::to_string(it->second.first) + "] ") : "* ";
            str += colorescape(it->second.second, options->colors.background);
            str += it->first;
            result.push_back(str);
        }
        return result;
    };
    auto longmenu = std::make_unique<LongMenu>(0, 1, 77, 20, 0, getterFunc, [](int itemId) {
        if (!g_dict_ctxt || itemId < 0 || itemId >= g_dict_ctxt->remaining.size()) return;
        auto it = g_dict_ctxt->remaining.begin();
        std::advance(it, itemId);
        selection = it->first;
        evbus->send(Events::UpdateWord {});
        evbus->send(Events::Route {"wordEntry"});
    });
    evbus->listen<Events::UpdateDict>([longmenu = longmenu.get()](const Events::UpdateDict&) {
        if (!g_dict_ctxt) return;
        longmenu->setCount(g_dict_ctxt->remaining.size());
        longmenu->resetPtr();
    });
    layout->add(std::move(longmenu));
    return layout;
}

std::unique_ptr<Layout> DictFilter() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        isFilter = false;
        g_dict_ctxt->apply();
        evbus->send(Events::UpdateDict {});
        evbus->send(Events::Route {"dictEntry"});
    }));
    auto mainText = std::make_unique<Text>(0, 1, 77, "");
    mainText->h = 19;
    static int lineId = 0, selectLine = 0, selectIndex = 0, selectChar = 0;
    evbus->listen<Events::ResetFilter>([](const Events::ResetFilter&) {
        lineId = selectLine = selectIndex = selectChar = 0;
        evbus->send(Events::UpdateFilterText {});
    });
    evbus->listen<Events::UpdateFilterText>([mainText = mainText.get()](const Events::UpdateFilterText&) {
        if (!g_dict_ctxt) {
            mainText->set("", 77);
            return;
        }
        std::stringstream ss;
        int count = 0, start = lineId * 13, end = std::min<int>(start + 13 * 19, (int) g_dict_ctxt->count());
        for (int rid = start; rid < end; rid++) {
            const auto& restriction = g_dict_ctxt->restrictions[rid];
            for (int i = 0; i < 5; i++) {
                ss << colorescape(
                    restriction.state[i]->second.color,
                    (rid == selectLine * 13 + selectIndex && i == selectChar) ?
                    options->colors.muted :
                    options->colors.background
                );
                ss << restriction.displayWord[i];
            }
            count++;
            if (rid != end - 1) {
                if (count == 13) {
                    ss << colorescape(options->colors.foreground, options->colors.background) << "\n";
                    count = 0;
                }
                else ss << colorescape(options->colors.foreground, options->colors.background) << " ";
            }
        }
        if (count) {
            count = 13 - count;
            ss << colorescape(options->colors.foreground, options->colors.background);
            for (int i = 0; i < count; i++) ss << "      ";
        }
        mainText->set(ss.str(), 77);
    });
    layout->add(std::move(mainText));
    layout->add(std::make_unique<Text>(0, 20, 77, tr(Msg::FilterHelp)));

    layout->setInput([](int ch) -> bool {
        if (!isFilter || !g_dict_ctxt) { return false; }
        if (ch == 'w' || ch == 'k' || ch == Key::Up) {
            if (!g_dict_ctxt->count()) { return true; }
            if (selectLine) {
                selectLine--;
                if (selectLine < lineId) lineId--;
            }
            else { selectIndex = 0, selectChar = 0; }
        }
        else if (ch == 's' || ch == 'j' || ch == Key::Down) {
            if (!g_dict_ctxt->count()) { return true; }
            if (selectLine < (g_dict_ctxt->count() - 1) / 13) {
                selectLine++;
                if (selectLine * 13 + selectIndex >= g_dict_ctxt->count()) {
                    selectIndex = g_dict_ctxt->count() - selectLine * 13 - 1;
                    selectChar = 4;
                }
                if (selectLine > lineId + 18) lineId++;
            }
            else {
                selectIndex = g_dict_ctxt->count() - selectLine * 13 - 1;
                selectChar = 4;
            }
        }
        else if (ch == 'a' || ch == 'h' || ch == Key::Left) {
            if (!g_dict_ctxt->count()) { return true; }
            if (selectChar) { selectChar--; }
            else if (selectIndex) { selectIndex--, selectChar = 4; }
            else if (selectLine) {
                selectLine--, selectIndex = 12, selectChar = 4;
                if (selectLine < lineId) lineId--;
            }
        }
        else if (ch == 'd' || ch == 'l' || ch == Key::Right) {
            if (!g_dict_ctxt->count()) { return true; }
            if (selectChar < 4) { selectChar++; }
            else if (selectLine * 13 + selectIndex + 1 < g_dict_ctxt->count()) {
                if (selectIndex < 12) { selectIndex++, selectChar = 0; }
                else {
                    selectLine++, selectIndex = 0, selectChar = 0;
                    if (selectLine > lineId + 18) lineId++;
                }
            }
        }
        else if (ch == 'A' || ch == 'H' || ch == Key::CtrlLeft) {
            if (!g_dict_ctxt->count()) { return true; }
            if (selectIndex) { selectIndex--; }
            else if (selectLine) {
                selectLine--, selectIndex = 12;
                if (selectLine < lineId) lineId--;
            }
            else { selectChar = 0; }
        }
        else if (ch == 'D' || ch == 'L' || ch == Key::CtrlRight) {
            if (!g_dict_ctxt->count()) { return true; }
            if (selectLine * 13 + selectIndex + 1 < g_dict_ctxt->count()) {
                if (selectIndex < 12) { selectIndex++; }
                else {
                    selectLine++, selectIndex = 0;
                    if (selectLine > lineId + 18) lineId++;
                }
            }
            else { selectChar = 4; }
        }
        else if (ch == '\r') {
            if (!g_dict_ctxt->count()) { return true; }
            g_dict_ctxt->next(selectLine * 13 + selectIndex, selectChar);
        }
        else if (ch == Key::Delete) {
            if (!g_dict_ctxt->count()) { return true; }
            isFilter = false;
            yesno(
                40, 8,
                tr(msg::FilterRemoveWord {g_dict_ctxt->restrictions[selectLine * 13 + selectIndex].word}),
                []() {
                    g_dict_ctxt->remove(selectLine * 13 + selectIndex);
                    if (!g_dict_ctxt->count()) {
                        selectLine = 0;
                        selectIndex = 0;
                        selectChar = 0;
                        lineId = 0;
                    }
                    else if (selectLine * 13 + selectIndex >= g_dict_ctxt->count()) {
                        selectLine = (g_dict_ctxt->count() - 1) / 13;
                        selectIndex = (g_dict_ctxt->count() - 1) % 13;
                        selectChar = 4;
                    }
                    evbus->send(Events::UpdateFilterText {});
                    isFilter = true;
                },
                []() { isFilter = true; }
            );

            return true;
        }
        else if (ch == 'p') {
            isFilter = false;
            yesno(
                40, 8,
                tr(Msg::SlotDeleteAllRestriction),
                []() {
                    int count = g_dict_ctxt->count();
                    for (int i = count - 1; i >= 0; i--) g_dict_ctxt->remove(i);

                    selectLine = 0;
                    selectIndex = 0;
                    selectChar = 0;
                    lineId = 0;
                    evbus->send(Events::UpdateFilterText {});
                    isFilter = true;
                },
                []() { isFilter = true; }
            );

            return true;
        }
        else if (ch == '\t') {
            isFilter = false;
            auto layout = std::make_unique<Layout>(20, 8, 40, 10, single_frame);
            layout->add(std::make_unique<Text>(2, 1, 37, tr(msg::SlotAddRestriction {"Tab"})));
            auto input = std::make_unique<Input>(2, 2, 22, false, "",
                tr(Msg::HintPleaseInput),
                [](char32_t ch) -> bool {
                    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z');
                }
            );
            auto hint = std::make_unique<Text>(2, 5, 22, tr(Msg::HintInvalidInput), 0);
            auto* inputRef = input.get();
            auto* hintRef = hint.get();
            layout->add(std::move(input));
            layout->add(std::move(hint));
            auto cancel = std::make_unique<Button>(2, 6, 16, 3, Key::Escape, []() {
                isFilter = true;
                evbus->send(Events::IndexPop {});
            });
            cancel->set(std::make_unique<Text>(0, 0, 12, tr(msg::ActionCancel {"Esc"})));
            layout->add(std::move(cancel));
            auto apply = std::make_unique<Button>(18, 6, 20, 3, '\r', [input = inputRef, hint = hintRef]() {
                if (!g_dict_ctxt) {
                    isFilter = true;
                    evbus->send(Events::IndexPop {});
                    return;
                }
                const auto displayContent = input->get();
                auto content = registry->dicts.at(g_dict_ctxt->parent).normalize(displayContent);
                if (!registry->dicts.at(g_dict_ctxt->parent).valid(content)) {
                    hint->color(options->colors.error);
                    return;
                }
                if (g_dict_ctxt->validate && !registry->dicts.at(g_dict_ctxt->parent).acceptable.count(content)) {
                    hint->color(options->colors.error);
                    return;
                }

                g_dict_ctxt->add(displayContent);
                isFilter = true;
                evbus->send(Events::IndexPop {});
                evbus->send(Events::UpdateFilterText {});
            });
            apply->set(std::make_unique<Text>(0, 0, 11, tr(msg::ActionConfirm {"Enter"})));
            layout->add(std::move(apply));
            evbus->send(Events::IndexPush {std::move(layout)});

            return true;
        }
        else return false;
        evbus->send(Events::UpdateFilterText {});
        return true;
    });
    return layout;
}

std::unique_ptr<Layout> WordViewer() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"dictMain"});
    }));
    auto word = std::make_unique<Text>(0, 1, 5, "");
    auto* wordRef = word.get();
    layout->add(std::move(word));
    evbus->listen<Events::UpdateWord>([word = wordRef](const Events::UpdateWord&) { word->set(selection, 5); });
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries;
    entries.push_back({tr(msg::EntryCopy {"C"}), []() {
        if (!copy_clipboard(selection)) {
            confirm(40, 8, tr(Msg::ErrorClipboard), []() {});
        }
    }, (int) 'c'});
    if (options->dict.search) {
        std::string str = tr(msg::SearchWith {search_engine, "S"});
        entries.push_back({str, []() {
            const auto& engines = options->dict.searchEngines;
            auto engine = engines.find(search_engine);
            if (engine == engines.end()) engine = engines.find(options->dict.searchEngine);
            if (engine == engines.end()) return;
            auto target = subst_search(engine->second, selection);
            if (!open_external_url(target)) confirm(40, 8, tr(Msg::ErrorBrowser), []() {});
        }, (int) 's'});
        entries.push_back({tr(msg::EntrySearchWith {"E"}), []() {
            evbus->send(Events::Route {"se_select"});
        }, (int) 'e'});
    }
    auto menu = std::make_unique<Menu>(0, 2, 77, entries);
    if (options->dict.search) {
        evbus->listen<Events::SearchEngineChanged>([menu = menu.get()](const Events::SearchEngineChanged&) {
            std::string str = tr(msg::SearchWith {search_engine, "S"});
            menu->set(1, str);
        });
    }
    layout->add(std::move(menu));
    return layout;
}

std::unique_ptr<Layout> SearchEngineSelect() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"wordEntry"});
    }));
    static int num_ses = options->dict.searchEngines.size();
    static int pages_ses = num_ses / 19 + !!(num_ses % 19);
    auto getterFunc = [](int pageId) -> std::vector<std::string> {
        if (pageId < 0 || pageId >= pages_ses) return {};
        auto it = options->dict.searchEngines.begin();
        std::advance(it, pageId * 19);
        std::vector<std::string> result;
        for (int i = 0; i < 19 && it != options->dict.searchEngines.end(); i++, it++) {
            result.push_back(it->first);
        }
        return result;
    };
    auto longmenu = std::make_unique<LongMenu>(0, 1, 77, 20, num_ses, getterFunc, [](int itemId) {
        if (itemId < 0 || itemId >= num_ses) return;
        auto it = options->dict.searchEngines.begin();
        std::advance(it, itemId);
        auto target = subst_search(it->second, selection);
        if (!open_external_url(target)) confirm(40, 8, tr(Msg::ErrorBrowser), []() {});
        evbus->send(Events::Route {"wordEntry"});
    });
    layout->add(std::move(longmenu));
    return layout;
}
