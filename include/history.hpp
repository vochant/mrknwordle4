#pragma once

#include "widgets/layout.hpp"
#include "widgets/text.hpp"
#include "widgets/link.hpp"
#include "widgets/longmenu.hpp"
#include "userdb.hpp"
#include "i18n.hpp"
#include "ipc.hpp"

int selectedHistory;

std::shared_ptr<Widget> HistoryMenu() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"main", 0});
    }));
    auto getterFunc = [](int page) -> std::vector<std::string> {
        int countEntries = count_history();
        if (page < 0 || page > countEntries / 19 + !!(countEntries % 19)) return {};

        std::vector<std::string> result = get_history(page * 19, 19);
        return result;
    };
    auto longmenu = std::make_shared<LongMenu>(0, 1, 77, 20, 0, getterFunc, [](int itemId) {
        int countEntries = count_history();
        if (itemId < 0 || itemId >= countEntries) return;

        ipc->send({"loadHistory", history_ids[itemId % 19]});
        ipc->send({"route", std::string("historyEntry")});
    });
    layout->add(longmenu);
    ipc->listen("updateHistory", [longmenu](Message msg) {
        longmenu->setcount(count_history());
        longmenu->resetptr();
    });
    return layout;
}

std::shared_ptr<Widget> HistoryEntry() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("historyMenu")});
    }));
    auto text = std::make_shared<Text>(0, 1, 77, "");
    ipc->listen("loadHistory", [text](Message msg) {
        int itemId = std::any_cast<int>(msg.payload);
        text->set(get_history_detail(itemId), 77);
    });
    layout->add(text);
    return layout;
}