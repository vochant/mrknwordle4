#pragma once

#include "widgets/layout.hpp"
#include "widgets/text.hpp"
#include "widgets/link.hpp"
#include "widgets/longmenu.hpp"
#include "i18n.hpp"
#include "ipc.hpp"
#include "options.hpp"

std::shared_ptr<Widget> PluginMenu() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"main", 0});
    }));
    static int num_plugins = options->pluginInfo.size();
    static int plg_pages = num_plugins / 19 + !!(num_plugins % 19);
    auto getterFunc = [](int page) -> std::vector<std::string> {
        if (page < 0 || page > plg_pages) return {};
        auto it = options->pluginInfo.begin();
        std::advance(it, page * 19);
        std::vector<std::string> result;
        for (int i = 0; i < 19 && it != options->pluginInfo.end(); i++, it++) {
            result.push_back(it->first);
        }
        return result;
    };
    auto longmenu = std::make_shared<LongMenu>(0, 1, 77, 20, num_plugins, getterFunc, [](int itemId) {
        if (itemId < 0 || itemId > num_plugins) return;
        auto it = options->pluginInfo.begin();
        std::advance(it, itemId);
        ipc->send({"pluginLoad", it->second});
        ipc->send({"route", std::string("pluginEntry")});
    });
    layout->add(longmenu);
    return layout;
}

std::shared_ptr<Widget> PluginEntry() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("pluginMenu")});
    }));
    auto text = std::make_shared<Text>(0, 1, 77, "");
    ipc->listen("pluginLoad", [text](Message msg) {
        text->set(std::any_cast<std::string>(msg.payload), 77);
    });
    layout->add(text);
    return layout;
}