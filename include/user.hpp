#pragma once

#ifndef WIN32_LEAN_AND_MEAN
# define WIN32_LEAN_AND_MEAN
#endif

#include "ipc.hpp"
#include "widgets/longmenu.hpp"
#include "widgets/menu.hpp"
#include "widgets/text.hpp"
#include "widgets/input.hpp"
#include "widgets/layout.hpp"
#include "widgets/link.hpp"
#include "widgets/button.hpp"
#include "i18n.hpp"
#include "util/dialog.hpp"
#include "userdb.hpp"

std::shared_ptr<Layout> UserMenuNotLogged() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"main", 0});
    }));
    layout->add(std::make_shared<Text>(0, 1, 77, translate("{hint.not_logged}")));
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries;
    entries.push_back({translate("{entry.login} (L)"), []() {
        ipc->send({"loginClear", 0});
        ipc->send({"route", std::string("userLogin")});
    }, (int) 'l'});
    entries.push_back({translate("{entry.register} (R)"), []() {
        ipc->send({"registerClear", 0});
        ipc->send({"route", std::string("userRegister")});
    }, (int) 'r'});
    auto menu = std::make_shared<Menu>(0, 2, 77, entries);
    layout->add(menu);
    return layout;
}

std::shared_ptr<Widget> UserMenuLogged() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"main", 0});
    }));
    auto welcome = std::make_shared<Text>(0, 1, 77, "");
    ipc->listen("updateUser", [welcome](Message msg) {
        if (!~uid) return;
        auto welcome_string = translate("{template.welcome}");
        char* target = new char[welcome_string.length() + username.length()];
        sprintf(target, welcome_string.c_str(), username.c_str());
        welcome->set(target, 77);
        delete[] target;
    });
    layout->add(welcome);
    std::vector<std::tuple<std::string, std::function<void()>, int>> entries;
    entries.push_back({translate("{entry.search_settings} (S)"), []() {
        ipc->send({"route", std::string("userSearch")});
    }, (int) 's'});
    entries.push_back({translate("{entry.security_settings} (E)"), []() {
        ipc->send({"securityClear", 0});
        ipc->send({"route", std::string("userSecurity")});
    }, (int) 'e'});
    entries.push_back({translate("{entry.logout} (L)"), []() {
        logout_user();
        ipc->send({"se-change", 0});
        ipc->send({"updateHistory", 0});
        ipc->send({"route", std::string("userMenuNL")});
    }, (int) 'l'});
    entries.push_back({translate("{entry.delete_account} (D)"), []() {
        ipc->send({"deleteClear", 0});
        ipc->send({"route", std::string("userDelete")});
    }, (int) 'd'});
    auto menu = std::make_shared<Menu>(0, 2, 77, entries);
    layout->add(menu);
    return layout;
}

std::shared_ptr<Widget> UserLogin() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("userMenuNL")});
    }));
    auto username = std::make_shared<Input>(14, 2, 48, false, "", translate("{slot.username}"), acceptIdentifier);
    layout->add(username);
    auto password = std::make_shared<Input>(14, 6, 48, true, "", translate("{slot.password}"), acceptAll, false);
    layout->add(password);
    ipc->listen("loginClear", [username, password](Message msg) {
        username->set("");
        password->set("");
    });
    username->setNext(password.get(), false);
    password->setNext(username.get(), true);
    auto login = std::make_shared<Button>(30, 17, 16, 3, '\r', [username, password]() {
        auto un_val = username->get(), pw_val = password->get();
        if (un_val == "") {
            confirm(40, 8, translate("{error.username_empty}"), []() {});
            return;
        }
        if (pw_val == "") {
            confirm(40, 12, translate("{error.password_empty}"), []() {});
            return;
        }
        int code = login_user(un_val, pw_val);
        if (code == 1) {
            confirm(40, 12, translate("{error.system_error}"), []() {});
            return;
        }
        if (code == 2) {
            confirm(40, 12, translate("{error.incorrect_credentials}"), []() {});
            return;
        }
        ipc->send({"updateUser", 0});
        ipc->send({"se-change", 0});
        ipc->send({"updateHistory", 0});
        ipc->send({"route", std::string("userMenuLD")});
    });
    login->set(std::make_shared<Text>(0, 0, 13, "LOGIN (Enter)"));
    layout->add(login);
    return layout;
}

std::shared_ptr<Widget> UserRegister() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("userMenuNL")});
    }));
    auto username = std::make_shared<Input>(14, 2, 48, false, "", translate("{slot.username}"), acceptIdentifier);
    layout->add(username);
    auto password = std::make_shared<Input>(14, 6, 48, true, "", translate("{slot.password}"), acceptAll, false);
    layout->add(password);
    auto repeat_password = std::make_shared<Input>(14, 10, 48, true, "", translate("{slot.repeat_password}"), acceptAll, false);
    layout->add(repeat_password);
    username->setNext(password.get(), false);
    password->setNext(repeat_password.get(), false);
    repeat_password->setNext(username.get(), true);
    ipc->listen("registerClear", [username, password, repeat_password](Message msg) {
        username->set("");
        password->set("");
        repeat_password->set("");
    });
    auto login = std::make_shared<Button>(29, 17, 19, 3, '\r', [username, password, repeat_password]() {
        auto un_val = username->get(), pw_val = password->get(), rpw_val = repeat_password->get();
        if (un_val == "") {
            confirm(40, 8, translate("{error.username_empty}"), []() {});
            return;
        }
        if (pw_val == "") {
            confirm(40, 12, translate("{error.password_empty}"), []() {});
            return;
        }
        if (pw_val != rpw_val) {
            confirm(40, 12, translate("{error.password_mismatch}"), []() {});
            return;
        }
        int code = create_user(un_val, pw_val);
        if (code == 1) {
            confirm(40, 12, translate("{error.system_error}"), []() {});
            return;
        }
        if (code == 2) {
            confirm(40, 12, translate("{error.user_exists}"), []() {});
            return;
        }
        ipc->send({"updateUser", 0});
        ipc->send({"updateHistory", 0});
        ipc->send({"route", std::string("userMenuLD")});
    });
    login->set(std::make_shared<Text>(0, 0, 16, "REGISTER (Enter)"));
    layout->add(login);
    return layout;
}

std::shared_ptr<Widget> UserSearch() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("userMenuLD")});
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
    auto longmenu = std::make_shared<LongMenu>(0, 1, 77, 20, num_ses, getterFunc, [](int id) {
        if (id < 0 || id > num_ses) return;
        if (!~uid) return;

        auto it = options->dictSearchEngines.begin();
        std::advance(it, id);
        if (!change_search_engine(it->first)) {
            confirm(40, 12, translate("{error.system_error}"), []() {});
            return;
        }
        ipc->send({"se-change", 0});
        ipc->send({"route", std::string("userMenuLD")});
    });
    layout->add(longmenu);
    return layout;
}

std::shared_ptr<Widget> UserSecurity() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("userMenuLD")});
    }));
    auto original_password = std::make_shared<Input>(14, 2, 48, true, "", translate("{slot.original_password}"));
    layout->add(original_password);
    auto username = std::make_shared<Input>(14, 6, 48, false, "", translate("{slot.new_username} ({hint.optional})"), acceptIdentifier, false);
    layout->add(username);
    auto password = std::make_shared<Input>(14, 10, 48, true, "", translate("{slot.new_password} ({hint.optional})"), acceptAll, false);
    layout->add(password);
    auto repeat_password = std::make_shared<Input>(14, 14, 48, true, "", translate("{slot.repeat_password} ({hint.optional})"), acceptAll, false);
    layout->add(repeat_password);
    original_password->setNext(username.get(), false);
    username->setNext(password.get(), false);
    password->setNext(repeat_password.get(), false);
    repeat_password->setNext(original_password.get(), true);
    ipc->listen("securityClear", [original_password, username, password, repeat_password](Message msg) {
        original_password->set("");
        username->set("");
        password->set("");
        repeat_password->set("");
    });
    auto login = std::make_shared<Button>(32, 17, 13, 3, '\r', [original_password, username, password, repeat_password]() {
        auto opw_val = original_password->get();
        if (!check_password(opw_val)) {
            confirm(40, 12, translate("{error.incorrect_credentials}"), []() {});
            return;
        }
        auto un_val = username->get(), pw_val = password->get(), rpw_val = repeat_password->get();
        if (un_val != "") {
            if (!change_username(un_val)) {
                confirm(40, 12, translate("{error.system_error}"), []() {});
                return;
            }
        }
        if (pw_val != "") {
            if (pw_val != rpw_val) {
                confirm(40, 12, translate("{error.password_mismatch}"), []() {});
                return;
            }
            if (!change_password(pw_val)) {
                confirm(40, 12, translate("{error.system_error}"), []() {});
                return;
            }
        }
        ipc->send({"updateUser", 0});
        ipc->send({"route", std::string("userMenuLD")});
    });
    login->set(std::make_shared<Text>(0, 0, 10, "OK (Enter)"));
    layout->add(login);
    return layout;
}

std::shared_ptr<Widget> UserDelete() {
    auto layout = std::make_shared<Layout>(0, 0, 77, 21, no_frame);
    layout->add(std::make_shared<Link>(0, 0, 12, translate("{hint.back} (Esc)"), VK_ESCAPE, []() {
        ipc->send({"route", std::string("userMenuLD")});
    }));
    layout->add(std::make_shared<Text>(2, 2, 73, translate("{hint.delete_account}"), 12));
    auto password = std::make_shared<Input>(14, 6, 48, true, "", translate("{slot.password}"));
    layout->add(password);
    ipc->listen("deleteClear", [password](Message msg) {
        password->set("");
    });
    auto remove = std::make_shared<Button>(32, 17, 13, 3, '\r', [password]() {
        auto pw_val = password->get();
        if (pw_val == "") {
            confirm(40, 12, translate("{error.password_empty}"), []() {});
            return;
        }
        if (!check_password(pw_val)) {
            confirm(40, 12, translate("{error.incorrect_credentials}"), []() {});
            return;
        }
        if (!remove_user()) {
            confirm(40, 12, translate("{error.system_error}"), []() {});
            return;
        }
        ipc->send({"se-change", 0});
        ipc->send({"updateHistory", 0});
        ipc->send({"route", std::string("userMenuNL")});
    });
    remove->set(std::make_shared<Text>(0, 0, 10, "OK (Enter)"));
    layout->add(remove);
    return layout;
}