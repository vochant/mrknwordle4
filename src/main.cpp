#include "env.hpp"
#include "options.hpp"
#include "services/keybd.service.hpp"
#include "services/mouse.service.hpp"
#include "ipc.hpp"
#include "render.hpp"
#include "i18n.hpp"
#include "game.hpp"
#include "dict.hpp"
#include "user.hpp"
#include "history.hpp"
#include "plugins.hpp"

#include <fstream>
#include <cstdio>
#include <thread>

#include "widgets/index.hpp"
#include "widgets/router.hpp"
#include "widgets/text.hpp"
#include "widgets/layout.hpp"
#include "widgets/menu.hpp"
#include "widgets/datetime.hpp"
#include "widgets/longmenu.hpp"
#include "widgets/link.hpp"

#include "userdb.hpp"

KeyboardService* keybd_service;
MouseService* mouse_service;
IPC* ipc;

PaintBrush* pb;

std::shared_ptr<Widget> MainMenu() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries;
    entries.push_back({translate("{entry.game} (G)"), []() { ipc->send({"route", std::string("gameEntry")}); }, (int) 'g'});
    entries.push_back({translate("{entry.history} (H)"), []() {
        if (~uid) ipc->send({"route", std::string("historyMenu")});
        else confirm(40, 8, translate("{hint.not_logged}"), []() {});
    }, (int) 'h'});
    entries.push_back({translate("{entry.user} (U)"), []() {
        if (~uid) ipc->send({"route", std::string("userMenuLD")});
        else ipc->send({"route", std::string("userMenuNL")});
    }, (int) 'u'});
    entries.push_back({translate("{entry.plugins} (P)"), []() {
        if (options->pluginEnabled) ipc->send({"route", std::string("pluginMenu")});
        else confirm(40, 8, translate("{hint.plugins_disabled}"), []() {});
    }, (int) 'p'});
    entries.push_back({translate("{entry.dictionary} (D)"), []() {
        if (g_dict_ctxt) ipc->send({"route", std::string("dictEntry")});
        else ipc->send({"route", std::string("dictInit1")});
    }, (int) 'd'});
    entries.push_back({translate("{entry.shutdown} (Esc)"), []() { ipc->send({"shutdown", 0}); }, VK_ESCAPE});
    auto menu = std::make_shared<Menu>(0, 0, 77, entries);
    layout->add(menu);
    return layout;
}

std::shared_ptr<Widget> GameMenu() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"main", 0});
    }));
    auto getter_func = [](int page) -> std::vector<std::string> {
        static int num_pages = options->gamemodes.size() / 19 + !!(options->gamemodes.size() % 19);
        if (page < 0 || page >= num_pages) return {};
        std::vector<std::string> result;
        auto it = options->gamemodes.begin();
        std::advance(it, page * 19);
        for (int i = 0; i < 19 && it != options->gamemodes.end(); i++, it++) {
            result.push_back(it->second.displayName);
        }
        return result;
    };
    auto longmenu = std::make_shared<LongMenu>(0, 1, 77, 20, options->gamemodes.size(), getter_func, [](int itemId) {
        if (itemId < 0 || itemId >= options->gamemodes.size()) return;
        auto it = options->gamemodes.begin();
        std::advance(it, itemId);
        selectedGamemode = it->first;
        ipc->send({"gameStart", &(it->second)});
    });
    layout->add(longmenu);
    return layout;
}

void managed_main() {
    pb = new PaintBrush();
    keybd_service = new KeyboardService();
    if (options->mouseControlling) mouse_service = new MouseService();
    ipc = new IPC();

    // 创建全局布局
	auto index = std::make_shared<Index>(0, 0, 80, 25); // 堆叠控件
    auto layout = std::make_shared<Layout>(0, 0, 80, 25, [](int x, int y, int w, int h, PaintBrush* pb) {
		pb->color(15, 0);
		pb->rect(x, y, x + w - 1, y + h - 1, false);
		pb->hline(x, x + w - 1, y + 2, {x, x + w - 1});
        pb->vline(x + 22, y, y + 2, {y, y + 2});
	});
    layout->add(std::make_shared<Text>(2, 1, 22, "MrknWordle 4.0-rc1"));
    layout->add(std::make_shared<DateTime>(24, 1));
    static auto router = std::make_shared<Router>(2, 3, 77, 21); // 切换控件/路由控件
    layout->add(router);
    index->push(layout);
	root_widget = index.get();

    ipc->listen("index-pop", [&index](Message msg) {
        index->pop();
    });

    ipc->listen("index-push", [&index](Message msg) {
        index->push(std::any_cast<std::shared_ptr<Layout>>(msg.payload));
    });

    if (options->handleInterrupt) {
        SetConsoleCtrlHandler([](DWORD signal) {
            switch (signal) {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
            case CTRL_CLOSE_EVENT:
            case CTRL_LOGOFF_EVENT:
            case CTRL_SHUTDOWN_EVENT:
                ipc->send({"shutdown", 0});
                return TRUE;
            default:
                return FALSE;
            }
        }, TRUE);
    }

    router->add("main", MainMenu());
    router->route("main");
    router->add("game", GameMain());
    router->add("gameEntry", GameMenu());
    router->add("dictInit1", DictSelector());
    router->add("dictInit2", JudgerSelector());
    router->add("dictEntry", DictMenu());
    router->add("dictMain", DictMain());
    router->add("dictFilter", DictFilter());
    router->add("wordEntry", WordViewer());
    router->add("se_select", SearchEngineSelect());
    router->add("userMenuNL", UserMenuNotLogged());
    router->add("userMenuLD", UserMenuLogged());
    router->add("userLogin", UserLogin());
    router->add("userRegister", UserRegister());
    router->add("userSearch", UserSearch());
    router->add("userSecurity", UserSecurity());
    router->add("userDelete", UserDelete());
    router->add("historyMenu", HistoryMenu());
    router->add("historyEntry", HistoryEntry());
    router->add("pluginMenu", PluginMenu());
    router->add("pluginEntry", PluginEntry());

    ipc->listen("route", [](Message msg) {
        router->route(std::any_cast<std::string>(msg.payload));
    });

    ipc->listen("main", [](Message msg) {
        router->route("main");
    });

    // 通过服务监听事件
	if (options->mouseControlling) mouse_service->listen([](int ix, int iy, bool isRight) {
		root_widget->onClick(ix, iy);
	});

	keybd_service->listen([](int k) {
		root_widget->onInput(k);
	});

    std::thread renderer_thread(renderer, pb);

    renderer_thread.join();

    delete keybd_service;
    if (options->mouseControlling) delete mouse_service;
    delete ipc;
    delete pb;
}

int configured_main() {
    InitCharset();
    Environment env;
    if (env.check()) return 1;
    managed_main();
    return 0;
}

int main() {
    std::ifstream ifs("config.json");
    if (!ifs) {
        fprintf(stderr, "Failed to open config.json\n");
        return 1;
    }
    json config;
    ifs >> config;
    options = new Options(config);
    options->load_plugins();
    options->post_load();
    if (!init_user_db()) {
        fprintf(stderr, "Failed to initialize user database\n");
        return 1;
    }
    int retval = configured_main();
    close_user_db();
    return retval;
}