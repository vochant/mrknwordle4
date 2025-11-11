#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
# define NOMINMAX
#endif

#include <mutex>
#include "dict_context.hpp"
#include "ipc.hpp"
#include "widgets/longmenu.hpp"
#include "widgets/menu.hpp"
#include "widgets/text.hpp"
#include "widgets/input.hpp"
#include "widgets/layout.hpp"
#include "widgets/link.hpp"
#include "widgets/button.hpp"
#include "i18n.hpp"
#include "services/keybd.service.hpp"
#include "options.hpp"
#include "logger.hpp"
#include <cstdio>
#include <windows.h>
#include <shellapi.h>
#include "userdb.hpp"

#include <sstream>
#include <cctype>
#include <codecvt>

#ifdef min
# undef min
#endif

void CopyTextToClipboard(const std::string& text8) {
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
    std::wstring text = converter.from_bytes(text8);

    if (!OpenClipboard(NULL)) {
        return;
    }

    EmptyClipboard();

    HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, (text.length() + 1) * sizeof(WCHAR));
    if (hglbCopy == NULL) {
        CloseClipboard();
        return;
    }

    LPWSTR lptstrCopy = static_cast<LPWSTR>(GlobalLock(hglbCopy));
    if (lptstrCopy == NULL) {
        GlobalFree(hglbCopy);
        CloseClipboard();
        return;
    }

    memcpy(lptstrCopy, text.c_str(), text.length() * sizeof(WCHAR));
    lptstrCopy[text.length()] = L'\0';

    GlobalUnlock(hglbCopy);

    SetClipboardData(CF_UNICODETEXT, hglbCopy);

    CloseClipboard();
}

std::string selectedDict, selectedWord;

std::shared_ptr<Layout> DictSelector() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"main", 0});
    }));
    int num_dicts = options->dictionaries.size();
    auto getterFunc = [num_dicts](int page) -> std::vector<std::string> {
        if (page < 0 || page * 19 > num_dicts) return {};
        auto it = options->dictionaries.begin();
        std::advance(it, page * 19);
        std::vector<std::string> result;
        for (int i = 0; i < 19 && it != options->dictionaries.end(); i++, it++) {
            result.push_back(it->first);
        }
        return result;
    };
    auto longmenu = std::make_shared<LongMenu>(0, 1, 77, 20, num_dicts, getterFunc, [num_dicts](int itemId) {
        if (itemId < 0 || itemId >= num_dicts) return;
        auto it = options->dictionaries.begin();
        std::advance(it, itemId);
        selectedDict = it->first;
        ipc->send({"route", std::string("dictInit2")});
    });
    layout->add(longmenu);
    return layout;
}

std::shared_ptr<Layout> JudgerSelector() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("dictInit1")});
    }));
    int num_determined_judgers = options->determinedJudgers.size();
    auto getterFunc = [num_determined_judgers](int page) -> std::vector<std::string> {
        if (page < 0 || page * 19 >= num_determined_judgers) return {};
        std::vector<std::string> result;
        for (int i = 0; i < 19 && (page * 19 + i) < num_determined_judgers; i++) {
            result.push_back(options->determinedJudgers[page * 19 + i].first);
        }
        return result;
    };
    auto longmenu = std::make_shared<LongMenu>(0, 1, 77, 20, num_determined_judgers, getterFunc, [num_determined_judgers](int itemId) {
        if (itemId < 0 || itemId >= num_determined_judgers) return;
        g_dict_ctxt = new DictContext(selectedDict, options->determinedJudgers[itemId].second);
        ipc->send({"updateDict", 0});
        ipc->send({"resetFilter", 0});
        ipc->send({"route", std::string("dictEntry")});
    });
    layout->add(longmenu);
    return layout;
}

bool isFilter = false;

std::shared_ptr<Layout> DictMenu() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries;
    entries.push_back({translate("{entry.view} (Enter)"), []() {
        ipc->send({"route", std::string("dictMain")});
    }, (int) '\r'});
    entries.push_back({translate("{entry.filter} (F)"), []() {
        isFilter = true;
        ipc->send({"route", std::string("dictFilter")});
    }, (int) 'f'});
    entries.push_back({translate("{entry.back} (Esc)"), []() {
        if (options->dictCleanup) {
            delete g_dict_ctxt;
            g_dict_ctxt = nullptr;
        }
        ipc->send({"main", 0});
    }, VK_ESCAPE});
    entries.push_back({translate("{entry.cleanup} (C)"), []() {
        delete g_dict_ctxt;
        g_dict_ctxt = nullptr;
        ipc->send({"route", std::string("dictInit1")});
    }, (int) 'c'});
    auto menu = std::make_shared<Menu>(0, 0, 77, entries);
    layout->add(menu);
    return layout;
}

std::shared_ptr<Layout> DictMain() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("dictEntry")});
    }));
    auto getterFunc = [](int page) -> std::vector<std::string> {
        if (!g_dict_ctxt) return {};
        if (page < 0 || page * 19 >= g_dict_ctxt->remaining.size()) return {};
        std::vector<std::string> result;
        auto it = g_dict_ctxt->remaining.begin();
        std::advance(it, page * 19);
        for (int i = 0; i < 19 && it != g_dict_ctxt->remaining.end(); i++, it++) {
            std::string str = options->dictShowId ? ("[" + std::to_string(it->second.first) + "] \x01") : "* \x01";
            str += it->second.second;
            str += it->first;
            result.push_back(str);
        }
        return result;
    };
    auto longmenu = std::make_shared<LongMenu>(0, 1, 77, 20, 0, getterFunc, [](int itemId) {
        if (itemId < 0 || itemId >= g_dict_ctxt->remaining.size()) return;
        auto it = g_dict_ctxt->remaining.begin();
        std::advance(it, itemId);
        selectedWord = it->first;
        ipc->send({"updateWord", 0});
        ipc->send({"route", std::string("wordEntry")});
    });
    ipc->listen("updateDict", [longmenu](Message msg) {
        if (!g_dict_ctxt) return;
        longmenu->setcount(g_dict_ctxt->remaining.size());
        longmenu->resetptr();
    });
    layout->add(longmenu);
    return layout;
}

std::shared_ptr<Layout> DictFilter() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        isFilter = false;
        g_dict_ctxt->applyRestrictions();
        ipc->send({"updateDict", 0});
        ipc->send({"route", std::string("dictEntry")});
    }));
    auto mainText = std::make_shared<Text>(0, 1, 77, "");
    static int lineId = 0, selectLine = 0, selectIndex = 0, selectChar = 0;
    ipc->listen("resetFilter", [](Message msg) {
        lineId = selectLine = selectIndex = selectChar = 0;
        ipc->send({"updateFilterText", 0});
    });
    ipc->listen("updateFilterText", [mainText](Message msg) {
        if (!g_dict_ctxt) {
            mainText->set("", 77);
            return;
        }
        std::stringstream ss;
        int count = 0, start = lineId * 13, end = std::min<int>(start + 13 * 19, (int) g_dict_ctxt->countRestrictions());
        for (int rid = start; rid < end; rid++) {
            const auto& restriction = g_dict_ctxt->restrictions[rid];
            for (int i = 0; i < 5; i++) {
                ss << '\x01' ;
                ss << (char) (restriction.state[i]->second.color | ((rid == selectLine * 13 + selectIndex && i == selectChar) ? 0x80 : 0));
                ss << restriction.word[i];
            }
            count++;
            if (rid != end - 1) {
                if (count == 13) ss << "\x01\x0f\n", count = 0;
                else ss << "\x01\x0f ";
            }
        }
        if (count) {
            count = 13 - count;
            ss << "\x01\x0f";
            for (int i = 0; i < count; i++) ss << "      ";
        }
        mainText->set(ss.str(), 77);
    });
    layout->add(mainText);
    layout->add(std::make_shared<Text>(0, 20, 77, translate("{hint.create}/Tab {hint.remove}/Delete {hint.empty}/P")));
    static std::mutex dict_lock;
    keybd_service->listen([](int ch) {
        dict_lock.lock();
        if (!isFilter) {
            dict_lock.unlock();
            return;
        }
        if (ch == 'w' || ch == 'k' || ch == 328) {
            if (!g_dict_ctxt->countRestrictions()) {
                dict_lock.unlock();
                return;
            }
            if (selectLine) {
                selectLine--;
                if (selectLine < lineId) lineId--;
            }
            else selectIndex = 0, selectChar = 0;
        }
        else if (ch == 's' || ch == 'j' || ch == 336) {
            if (!g_dict_ctxt->countRestrictions()) {
                dict_lock.unlock();
                return;
            }
            if (selectLine < (g_dict_ctxt->countRestrictions() - 1) / 13) {
                selectLine++;
                if (selectLine * 13 + selectIndex >= g_dict_ctxt->countRestrictions()) {
                    selectIndex = g_dict_ctxt->countRestrictions() - selectLine * 13 - 1;
                    selectChar = 4;
                }
                if (selectLine > lineId + 18) lineId++;
            }
            else {
                selectIndex = g_dict_ctxt->countRestrictions() - selectLine * 13 - 1;
                selectChar = 4;
            }
        }
        else if (ch == 'a' || ch == 'h' || ch == 331) {
            if (!g_dict_ctxt->countRestrictions()) {
                dict_lock.unlock();
                return;
            }
            if (selectChar) selectChar--;
            else if (selectIndex) selectIndex--, selectChar = 4;
            else if (selectLine) {
                selectLine--, selectIndex = 12, selectChar = 4;
                if (selectLine < lineId) lineId--;
            }
        }
        else if (ch == 'd' || ch == 'l' || ch == 333) {
            if (!g_dict_ctxt->countRestrictions()) {
                dict_lock.unlock();
                return;
            }
            if (selectChar < 4) selectChar++;
            else if (selectLine * 13 + selectIndex + 1 < g_dict_ctxt->countRestrictions()) {
                if (selectIndex < 12) selectIndex++, selectChar = 0;
                else {
                    selectLine++, selectIndex = 0, selectChar = 0;
                    if (selectLine > lineId + 18) lineId++;
                }
            }
        }
        else if (ch == 'A' || ch == 'H' || ch == 371) {
            if (!g_dict_ctxt->countRestrictions()) {
                dict_lock.unlock();
                return;
            }
            if (selectIndex) selectIndex--;
            else if (selectLine) {
                selectLine--, selectIndex = 12;
                if (selectLine < lineId) lineId--;
            }
            else selectChar = 0;
        }
        else if (ch == 'D' || ch == 'L' || ch == 372) {
            if (!g_dict_ctxt->countRestrictions()) {
                dict_lock.unlock();
                return;
            }
            if (selectLine * 13 + selectIndex + 1 < g_dict_ctxt->countRestrictions()) {
                if (selectIndex < 12) selectIndex++;
                else {
                    selectLine++, selectIndex = 0;
                    if (selectLine > lineId + 18) lineId++;
                }
            }
            else selectChar = 4;
        }
        else if (ch == '\r') {
            if (!g_dict_ctxt->countRestrictions()) {
                dict_lock.unlock();
                return;
            }
            g_dict_ctxt->increaseRestriction(selectLine * 13 + selectIndex, selectChar);
        }
        else if (ch == 339) {
            if (!g_dict_ctxt->countRestrictions()) {
                dict_lock.unlock();
                return;
            }
            isFilter = false;
            yesno(40, 8, translate("{slot.delete_restriction} ") + g_dict_ctxt->restrictions[selectLine * 13 + selectIndex].word, []() {
                dict_lock.lock();
                g_dict_ctxt->removeRestriction(selectLine * 13 + selectIndex);
                if (!g_dict_ctxt->countRestrictions()) {
                    selectLine = 0;
                    selectIndex = 0;
                    selectChar = 0;
                    lineId = 0;
                }
                else if (selectLine * 13 + selectIndex >= g_dict_ctxt->countRestrictions()) {
                    selectLine = (g_dict_ctxt->countRestrictions() - 1) / 13;
                    selectIndex = (g_dict_ctxt->countRestrictions() - 1) % 13;
                    selectChar = 4;
                }
                ipc->send({"updateFilterText", 0});
                isFilter = true;
                dict_lock.unlock();
            }, []() {
                dict_lock.lock();
                isFilter = true;
                dict_lock.unlock();
            });
            dict_lock.unlock();
            return;
        }
        else if (ch == 'p') {
            isFilter = false;
            yesno(40, 8, translate("{slot.delete_all_restriction}"), []() {
                int count = g_dict_ctxt->countRestrictions();
                for (int i = count - 1; i >= 0; i--) g_dict_ctxt->removeRestriction(i);
                dict_lock.lock();
                selectLine = 0;
                selectIndex = 0;
                selectChar = 0;
                lineId = 0;
                ipc->send({"updateFilterText", 0});
                isFilter = true;
                dict_lock.unlock();
            }, []() {
                dict_lock.lock();
                isFilter = true;
                dict_lock.unlock();
            });
            dict_lock.unlock();
            return;
        }
        else if (ch == '\t') {
            isFilter = false;
            auto layout = std::make_shared<Layout>(20, 8, 40, 10, single_frame);
            layout->add(std::make_shared<Text>(2, 1, 37, translate("{slot.add_restriction} (Tab)")));
            auto input = std::make_shared<Input>(1, 2, 23, false, "", translate("{hint.please_input}"), [](char ch) -> bool {
                return std::islower(ch);
            });
            auto hint = std::make_shared<Text>(2, 5, 12, translate("{hint.invalid_input}"), 0);
            layout->add(input);
            layout->add(hint);
            auto cancel = std::make_shared<Button>(1, 6, 15, 3, VK_ESCAPE, []() {
                dict_lock.lock();
                isFilter = true;
                ipc->send({"index-pop", nullptr});
                dict_lock.unlock();
            });
            cancel->set(std::make_shared<Text>(0, 0, 12, "CANCEL (Esc)"));
            layout->add(cancel);
            auto apply = std::make_shared<Button>(26, 6, 13, 3, '\r', [input, hint]() {
                auto content = input->get();
                if (!basicValidation(content)) {
                    hint->color(12);
                    return;
                }
                if (options->dictValidation && !options->fullDictionary.count(content)) {
                    hint->color(12);
                    return;
                }
                dict_lock.lock();
                g_dict_ctxt->addRestriction(content);
                isFilter = true;
                ipc->send({"index-pop", nullptr});
                ipc->send({"updateFilterText", 0});
                dict_lock.unlock();
            });
            apply->set(std::make_shared<Text>(0, 0, 11, "OK (Enter)"));
            layout->add(apply);
            ipc->send({"index-push", layout});
            dict_lock.unlock();
            return;
        }
        else {
            dict_lock.unlock();
            return;
        }
        ipc->send({"updateFilterText", 0});
        dict_lock.unlock();
    });
    return layout;
}

std::shared_ptr<Layout> WordViewer() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("dictMain")});
    }));
    auto word = std::make_shared<Text>(0, 1, 5, "");
    layout->add(word);
    ipc->listen("updateWord", [word](Message msg) {
        word->set(selectedWord, 5);
    });
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries;
    entries.push_back({translate("{entry.copy} (C)"), []() {
        CopyTextToClipboard(selectedWord);
    }, (int) 'c'});
    if (options->dictSearch) {
        auto search_with = translate("{template.search_with} (S)");
        char* target = new char[search_with.length() + search_engine.length() - 1];
        std::sprintf(target, search_with.c_str(), search_engine.c_str());
        std::string str = target;
        delete target;
        entries.push_back({str, []() {
            char* target = new char[options->dictSearchEngines[search_engine].length() + 4];
            std::sprintf(target, options->dictSearchEngines[search_engine].c_str(), selectedWord.c_str());
            ShellExecuteA(NULL, "open", target, NULL, NULL, SW_SHOWNORMAL);
            delete target;
        }, (int) 's'});
        entries.push_back({translate("{entry.search_with} (E)"), []() {
            ipc->send({"route", std::string("se_select")});
        }, (int) 'e'});
    }
    auto menu = std::make_shared<Menu>(0, 2, 77, entries);
    if (options->dictSearch) {
        ipc->listen("se-change", [menu](Message msg) {
            auto search_with = translate("{template.search_with} (S)");
            char* target = new char[search_with.length() + search_engine.length() - 1];
            std::sprintf(target, search_with.c_str(), search_engine.c_str());
            std::string str = target;
            delete target;
            menu->set(1, str);
        });
    }
    layout->add(menu);
    return layout;
}

std::shared_ptr<Layout> SearchEngineSelect() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("wordEntry")});
    }));
    static int num_ses = options->dictSearchEngines.size();
    static int pages_ses = num_ses / 19 + !!(num_ses % 19);
    auto getterFunc = [](int pageId) -> std::vector<std::string> {
        if (pageId < 0 || pageId > pages_ses) return {};
        auto it = options->dictSearchEngines.begin();
        std::advance(it, pageId * 19);
        std::vector<std::string> result;
        for (int i = 0; i < 19 && it != options->dictSearchEngines.end(); i++, it++) {
            result.push_back(it->first);
        }
        return result;
    };
    auto longmenu = std::make_shared<LongMenu>(0, 1, 77, 20, num_ses, getterFunc, [](int itemId) {
        if (itemId < 0 || itemId > num_ses) return;
        auto it = options->dictSearchEngines.begin();
        std::advance(it, itemId);
        char* target = new char[it->second.length() + 4];
        std::sprintf(target, it->second.c_str(), selectedWord.c_str());
        ShellExecuteA(NULL, "open", target, NULL, NULL, SW_SHOWNORMAL);
        delete target;
        ipc->send({"route", std::string("wordEntry")});
    });
    layout->add(longmenu);
    return layout;
}