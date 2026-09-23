#include "ui/screens.hpp"
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
#include "util/dialog.hpp"
#include "userdb.hpp"

std::unique_ptr<Layout> UserMenuNotLogged() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Main {});
    }));
    layout->add(std::make_unique<Text>(0, 1, 77, tr(Msg::HintNotLogged)));
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries;
    entries.push_back({tr(msg::EntryLogin {"L"}), []() {
        evbus->send(Events::LoginClear {});
        evbus->send(Events::Route {"userLogin"});
    }, (int) 'l'});
    entries.push_back({tr(msg::EntryRegister {"R"}), []() {
        evbus->send(Events::RegisterClear {});
        evbus->send(Events::Route {"userRegister"});
    }, (int) 'r'});
    auto menu = std::make_unique<Menu>(0, 2, 77, entries);
    layout->add(std::move(menu));
    return layout;
}

std::unique_ptr<Widget> UserMenuLogged() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Main {});
    }));
    auto welcome = std::make_unique<Text>(0, 1, 77, "");
    Text* welcomeRef = welcome.get();
    evbus->listen<Events::UpdateUser>([welcome = welcomeRef](const Events::UpdateUser&) {
        if (!~uid) return;
        welcome->set(tr(msg::TemplateWelcome {username}), 77);
    });
    layout->add(std::move(welcome));
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries;
    entries.push_back({tr(msg::EntrySearchSettings {"S"}), []() {
        evbus->send(Events::Route {"userSearch"});
    }, (int) 's'});
    entries.push_back({tr(msg::EntrySecuritySettings {"E"}), []() {
        evbus->send(Events::SecurityClear {});
        evbus->send(Events::Route {"userSecurity"});
    }, (int) 'e'});
    entries.push_back({tr(msg::EntryLogout {"L"}), []() {
        logout_user();
        evbus->send(Events::SearchEngineChanged {});
        evbus->send(Events::UpdateHistory {});
        evbus->send(Events::Route {"userMenuNL"});
    }, (int) 'l'});
    entries.push_back({tr(msg::EntryDeleteAccount {"D"}), []() {
        evbus->send(Events::DeleteClear {});
        evbus->send(Events::Route {"userDelete"});
    }, (int) 'd'});
    auto menu = std::make_unique<Menu>(0, 2, 77, entries);
    layout->add(std::move(menu));
    return layout;
}

std::unique_ptr<Widget> UserLogin() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"userMenuNL"});
    }));
    auto username = std::make_unique<Input>(14, 2, 48, false, "", tr(Msg::SlotUsername), accept_identifier);
    Input* usernameRef = username.get();
    layout->add(std::move(username));
    auto password = std::make_unique<Input>(14, 6, 48, true, "", tr(Msg::SlotPassword), accept_all, false);
    Input* passwordRef = password.get();
    layout->add(std::move(password));
    evbus->listen<Events::LoginClear>([username = usernameRef, password = passwordRef](const Events::LoginClear&) {
        username->set("");
        password->set("");
    });
    auto login = std::make_unique<Button>(26, 17, 26, 3, '\r', [username = usernameRef, password = passwordRef]() {
        auto un_val = username->get(), pw_val = password->get();
        if (un_val == "") {
            confirm(40, 8, tr(Msg::ErrorUsernameEmpty), []() {});
            return;
        }
        if (pw_val == "") {
            confirm(40, 12, tr(Msg::ErrorPasswordEmpty), []() {});
            return;
        }
        int code = login_user(un_val, pw_val);
        if (code == 1) {
            confirm(40, 12, tr(Msg::ErrorSystemError), []() {});
            return;
        }
        if (code == 2) {
            confirm(40, 12, tr(Msg::ErrorIncorrectCredentials), []() {});
            return;
        }
        evbus->send(Events::UpdateUser {});
        evbus->send(Events::SearchEngineChanged {});
        evbus->send(Events::UpdateHistory {});
        evbus->send(Events::Route {"userMenuLD"});
    });
    login->set(std::make_unique<Text>(0, 0, 22, tr(msg::EntryLogin {"Enter"})));
    layout->add(std::move(login));
    return layout;
}

std::unique_ptr<Widget> UserRegister() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"userMenuNL"});
    }));
    auto username = std::make_unique<Input>(14, 2, 48, false, "", tr(Msg::SlotUsername), accept_identifier);
    Input* usernameRef = username.get();
    layout->add(std::move(username));
    auto password = std::make_unique<Input>(14, 6, 48, true, "", tr(Msg::SlotPassword), accept_all, false);
    Input* passwordRef = password.get();
    layout->add(std::move(password));
    auto repeat_password = std::make_unique<Input>(14, 10, 48, true, "", tr(Msg::SlotRepeatPassword), accept_all, false);
    Input* repeatPasswordRef = repeat_password.get();
    layout->add(std::move(repeat_password));
    evbus->listen<Events::RegisterClear>([
        username = usernameRef,
        password = passwordRef,
        repeat_password = repeatPasswordRef
    ](const Events::RegisterClear&) {
        username->set("");
        password->set("");
        repeat_password->set("");
    });
    auto login = std::make_unique<Button>(24, 17, 30, 3, '\r', [
        username = usernameRef,
        password = passwordRef,
        repeat_password = repeatPasswordRef
    ]() {
        auto un_val = username->get(), pw_val = password->get(), rpw_val = repeat_password->get();
        if (un_val == "") {
            confirm(40, 8, tr(Msg::ErrorUsernameEmpty), []() {});
            return;
        }
        if (pw_val == "") {
            confirm(40, 12, tr(Msg::ErrorPasswordEmpty), []() {});
            return;
        }
        if (pw_val != rpw_val) {
            confirm(40, 12, tr(Msg::ErrorPasswordMismatch), []() {});
            return;
        }
        int code = create_user(un_val, pw_val);
        if (code == 1) {
            confirm(40, 12, tr(Msg::ErrorSystemError), []() {});
            return;
        }
        if (code == 2) {
            confirm(40, 12, tr(Msg::ErrorUserExists), []() {});
            return;
        }
        evbus->send(Events::UpdateUser {});
        evbus->send(Events::UpdateHistory {});
        evbus->send(Events::Route {"userMenuLD"});
    });
    login->set(std::make_unique<Text>(0, 0, 26, tr(msg::EntryRegister {"Enter"})));
    layout->add(std::move(login));
    return layout;
}

std::unique_ptr<Widget> UserSearch() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"userMenuLD"});
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
    auto longmenu = std::make_unique<LongMenu>(0, 1, 77, 20, num_ses, getterFunc, [](int id) {
        if (id < 0 || id >= num_ses) return;
        if (!~uid) return;

        auto it = options->dict.searchEngines.begin();
        std::advance(it, id);
        if (!change_search_engine(it->first)) {
            confirm(40, 12, tr(Msg::ErrorSystemError), []() {});
            return;
        }
        evbus->send(Events::SearchEngineChanged {});
        evbus->send(Events::Route {"userMenuLD"});
    });
    layout->add(std::move(longmenu));
    return layout;
}

std::unique_ptr<Widget> UserSecurity() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"userMenuLD"});
    }));
    auto original_password = std::make_unique<Input>(14, 2, 48, true, "", tr(Msg::SlotOriginalPassword));
    Input* originalPasswordRef = original_password.get();
    layout->add(std::move(original_password));
    auto username = std::make_unique<Input>(14, 6, 48, false, "", tr(Msg::NewUsernameLabel), accept_identifier, false);
    Input* usernameRef = username.get();
    layout->add(std::move(username));
    auto password = std::make_unique<Input>(14, 10, 48, true, "", tr(Msg::NewPasswordLabel), accept_all, false);
    Input* passwordRef = password.get();
    layout->add(std::move(password));
    auto repeat_password = std::make_unique<Input>(14, 14, 48, true, "", tr(Msg::RepeatPasswordlLabel), accept_all, false);
    Input* repeatPasswordRef = repeat_password.get();
    layout->add(std::move(repeat_password));
    evbus->listen<Events::SecurityClear>([
        original_password = originalPasswordRef,
        username = usernameRef,
        password = passwordRef,
        repeat_password = repeatPasswordRef
    ](const Events::SecurityClear&) {
        original_password->set("");
        username->set("");
        password->set("");
        repeat_password->set("");
    });
    auto login = std::make_unique<Button>(28, 17, 20, 3, '\r', [
        original_password = originalPasswordRef,
        username = usernameRef,
        password = passwordRef,
        repeat_password = repeatPasswordRef
    ]() {
        auto opw_val = original_password->get();
        if (!check_password(opw_val)) {
            confirm(40, 12, tr(Msg::ErrorIncorrectCredentials), []() {});
            return;
        }
        auto un_val = username->get(), pw_val = password->get(), rpw_val = repeat_password->get();
        if (un_val != "") {
            if (!change_username(un_val)) {
                confirm(40, 12, tr(Msg::ErrorSystemError), []() {});
                return;
            }
        }
        if (pw_val != "") {
            if (pw_val != rpw_val) {
                confirm(40, 12, tr(Msg::ErrorPasswordMismatch), []() {});
                return;
            }
            if (!change_password(pw_val)) {
                confirm(40, 12, tr(Msg::ErrorSystemError), []() {});
                return;
            }
        }
        evbus->send(Events::UpdateUser {});
        evbus->send(Events::Route {"userMenuLD"});
    });
    login->set(std::make_unique<Text>(0, 0, 10, tr(msg::ActionConfirm {"Enter"})));
    layout->add(std::move(login));
    return layout;
}

std::unique_ptr<Widget> UserDelete() {
    auto layout = std::make_unique<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_unique<Link>(0, 0, 16, tr(msg::HintBack {"Esc"}), Key::Escape, []() {
        evbus->send(Events::Route {"userMenuLD"});
    }));
    auto warning = std::make_unique<Text>(2, 2, 73, tr(Msg::HintDeleteAccount), options->colors.error);
    warning->h = 3;
    layout->add(std::move(warning));
    auto password = std::make_unique<Input>(14, 6, 48, true, "", tr(Msg::SlotPassword));
    Input* passwordRef = password.get();
    layout->add(std::move(password));
    evbus->listen<Events::DeleteClear>([password = passwordRef](const Events::DeleteClear&) { password->set(""); });
    auto remove = std::make_unique<Button>(28, 17, 20, 3, '\r', [password = passwordRef]() {
        auto pw_val = password->get();
        if (pw_val == "") {
            confirm(40, 12, tr(Msg::ErrorPasswordEmpty), []() {});
            return;
        }
        if (!check_password(pw_val)) {
            confirm(40, 12, tr(Msg::ErrorIncorrectCredentials), []() {});
            return;
        }
        if (!remove_user()) {
            confirm(40, 12, tr(Msg::ErrorSystemError), []() {});
            return;
        }
        evbus->send(Events::SearchEngineChanged {});
        evbus->send(Events::UpdateHistory {});
        evbus->send(Events::Route {"userMenuNL"});
    });
    remove->set(std::make_unique<Text>(0, 0, 10, tr(msg::ActionConfirm {"Enter"})));
    layout->add(std::move(remove));
    return layout;
}
