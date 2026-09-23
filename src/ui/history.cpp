#include "ui/screens.hpp"
#include "widgets/layout.hpp"
#include "widgets/text.hpp"
#include "widgets/link.hpp"
#include "widgets/longmenu.hpp"
#include "userdb.hpp"
#include "i18n.hpp"
#include "eventbus.hpp"

std::unique_ptr<Widget> HistoryMenu() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Main {});
    }));
    auto getterFunc = [](int page) -> std::vector<std::string> {
        int countEntries = count_history();
        if (page < 0 || page > countEntries / 19 + !!(countEntries % 19)) return {};

        std::vector<std::string> result = get_history(page * 19, 19);
        return result;
    };
    auto longmenu = std::make_unique<LongMenu>(0, 1, 77, 20, 0, getterFunc, [](int itemId) {
        int countEntries = count_history();
        if (itemId < 0 || itemId >= countEntries) return;

        evbus->send(Events::LoadHistory {history_ids[itemId % 19]});
        evbus->send(Events::Route {"historyEntry"});
    });
    auto* longmenuRef = longmenu.get();
    evbus->listen<Events::UpdateHistory>([longmenu = longmenuRef](const Events::UpdateHistory&) {
        longmenu->setCount(count_history());
        longmenu->resetPtr();
    });
    layout->add(std::move(longmenu));
    return layout;
}

std::unique_ptr<Widget> HistoryEntry() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"historyMenu"});
    }));
    auto text = std::make_unique<Text>(0, 1, 77, "");
    evbus->listen<Events::LoadHistory>([text = text.get()](const Events::LoadHistory& msg) {
        int itemId = msg.id;
        text->set(get_history_detail(itemId), 77);
    });
    text->h = 19;
    layout->add(std::move(text));
    return layout;
}
