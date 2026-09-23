#include "ui/application.hpp"
#include "ui/screens.hpp"
#include "options.hpp"
#include "eventbus.hpp"
#include "render.hpp"
#include "game_context.hpp"
#include "dict_context.hpp"
#include "services/keybd.service.hpp"
#include "services/mouse.service.hpp"
#include "widgets/index.hpp"
#include "widgets/router.hpp"
#include "widgets/text.hpp"
#include "widgets/layout.hpp"
#include "widgets/datetime.hpp"

KeyboardService* keybd_service = nullptr;
MouseService* mouse_service = nullptr;
EventBus* evbus = nullptr;

void run_app() {
    PaintBrush brush;
    EventBus bus;
    KeyboardService keyboard;
    MouseService mouse;
    evbus = &bus;
    keybd_service = &keyboard;
    mouse_service = options->term.mouse ? &mouse : nullptr;
    struct Cleanup {
        ~Cleanup() {
            evbus->close();
            delete g_context;
            g_context = nullptr;
            delete g_dict_ctxt;
            g_dict_ctxt = nullptr;
            root_widget = nullptr;
            keybd_service = nullptr;
            mouse_service = nullptr;
            evbus = nullptr;
        }
    } cleanup;
    auto index = std::make_unique<Index>(0, 0, 80, 25);
    auto layout = std::make_unique<Layout>(0, 0, 80, 25, [](int x, int y, int w, int h, PaintBrush* pb) {
        pb->color(options->colors.foreground, options->colors.background);
        pb->rect(x, y, x + w, y + h - 1, false);
        pb->hline(x, x + w, y + 2, {x, x + 20, x + w});
        pb->vline(x + 20, y, y + 2, {y, y + 2});
    });
    layout->add(std::make_unique<Text>(2, 1, 18, "MrknWordle " WORDLE_VERSION));
    layout->add(std::make_unique<DateTime>(22, 1));
    auto router = std::make_unique<Router>(2, 3, 77, 21);
    auto* routerPtr = router.get();
    layout->add(std::move(router));
    index->push(std::move(layout));
    root_widget = index.get();

    evbus->listen<Events::IndexPop>([index = index.get()](const Events::IndexPop&) {
        index->pop();
    });
    evbus->listen<Events::IndexPush>([index = index.get()](Events::IndexPush& msg) {
        index->push(std::move(msg.layout));
    });

    routerPtr->add("main", MainMenu());
    routerPtr->route("main");
    routerPtr->add("game", GameMain());
    routerPtr->add("gameEntry", GameMenu());
    routerPtr->add("gameAdvanced", GameAdvancedMenu());
    routerPtr->add("dictInit1", DictSelector());
    routerPtr->add("dictAdvanced", DictAdvancedSelector());
    routerPtr->add("dictEntry", DictMenu());
    routerPtr->add("dictMain", DictMain());
    routerPtr->add("dictFilter", DictFilter());
    routerPtr->add("wordEntry", WordViewer());
    routerPtr->add("se_select", SearchEngineSelect());
    routerPtr->add("userMenuNL", UserMenuNotLogged());
    routerPtr->add("userMenuLD", UserMenuLogged());
    routerPtr->add("userLogin", UserLogin());
    routerPtr->add("userRegister", UserRegister());
    routerPtr->add("userSearch", UserSearch());
    routerPtr->add("userSecurity", UserSecurity());
    routerPtr->add("userDelete", UserDelete());
    routerPtr->add("historyMenu", HistoryMenu());
    routerPtr->add("historyEntry", HistoryEntry());
    routerPtr->add("pluginMenu", PluginMenu());
    routerPtr->add("pluginEntry", PluginEntry());

    evbus->listen<Events::Route>([routerPtr](const Events::Route& msg) {
        routerPtr->route(msg.name);
    });
    evbus->listen<Events::Main>([routerPtr](const Events::Main&) {
        routerPtr->route("main");
    });
    if (options->term.mouse) {
        mouse_service->listen([](int ix, int iy, bool) {
            root_widget->onClick(ix, iy);
        });
    }
    keybd_service->setHandler([](int k) {
        return root_widget && root_widget->onInput(k);
    });

    renderer(&brush);
}
