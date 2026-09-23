#include "ui/screens.hpp"
#include "widgets/layout.hpp"
#include "widgets/text.hpp"
#include "widgets/link.hpp"
#include "widgets/longmenu.hpp"
#include "i18n.hpp"
#include "eventbus.hpp"
#include "plugins/manager.hpp"

std::unique_ptr<Widget> PluginMenu() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Main {});
    }));
    const int num_plugins =pluginManager->entries().size();
    auto getterFunc = [](int page) -> std::vector<std::string> {
        const auto& entries = pluginManager->entries();
        if (page < 0 || page >= (entries.size() + 18) / 19) return {};
        auto it = entries.begin() + page * 19;
        std::vector<std::string> result;
        for (int index = 0; index < 19 && it != entries.end(); index++, it++) {
            result.push_back(it->name + (it->status == PluginStatus::Failed ? " [" + tr(Msg::PluginFailed) + "]" : ""));
        }
        return result;
    };
    auto longmenu = std::make_unique<LongMenu>(0, 1, 77, 20, num_plugins, getterFunc, [](int itemId) {
        const auto& entries = pluginManager->entries();
        if (itemId < 0 || itemId >= entries.size()) return;
        evbus->send(Events::PluginLoad {entries[itemId].summary()});
        evbus->send(Events::Route {"pluginEntry"});
    });
    layout->add(std::move(longmenu));
    return layout;
}

std::unique_ptr<Widget> PluginEntry() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"pluginMenu"});
    }));
    auto text = std::make_unique<Text>(0, 1, 77, "");
    evbus->listen<Events::PluginLoad>([text = text.get()](const Events::PluginLoad& msg) { text->set(msg.text, 77); });
    text->h = 19;
    layout->add(std::move(text));
    return layout;
}
