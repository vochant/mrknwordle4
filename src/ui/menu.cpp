#include "registry.hpp"
#include "ui/screens.hpp"
#include "eventbus.hpp"
#include "options.hpp"
#include "dict_context.hpp"
#include "userdb.hpp"
#include "i18n.hpp"
#include "util/dialog.hpp"
#include "widgets/layout.hpp"
#include "widgets/menu.hpp"

std::unique_ptr<Widget> MainMenu() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries;
    entries.push_back({tr(msg::EntryGame {"G"}), []() {
        resetGameSetup();
        evbus->send(Events::Route {"gameEntry"});
    }, (int) 'g'});
    entries.push_back({tr(msg::EntryHistory {"H"}), []() {
        if (~uid) evbus->send(Events::Route {"historyMenu"});
        else confirm(40, 8, tr(Msg::HintNotLogged), []() {});
    }, (int) 'h'});
    entries.push_back({tr(msg::EntryUser {"U"}), []() {
        if (~uid) evbus->send(Events::Route {"userMenuLD"});
        else evbus->send(Events::Route {"userMenuNL"});
    }, (int) 'u'});
    entries.push_back({tr(msg::EntryPlugins {"P"}), []() {
        if (options->plugins.enabled) { evbus->send(Events::Route {"pluginMenu"}); }
        else confirm(40, 8, tr(Msg::HintPluginsDisabled), []() {});
    }, (int) 'p'});
    entries.push_back({tr(msg::EntryDictionary {"D"}),[]() {
        if (g_dict_ctxt) evbus->send(Events::Route {"dictEntry"});
        else {
            resetDictionarySetup();
            evbus->send(Events::Route {"dictInit1"});
        }
    }, (int) 'd'});
    entries.push_back({tr(msg::EntryShutdown {"Esc"}), []() {
        evbus->send(Events::Shutdown {});
    }, Key::Escape});
    auto menu = std::make_unique<Menu>(0, 0, 77, entries);
    layout->add(std::move(menu));
    return layout;
}
