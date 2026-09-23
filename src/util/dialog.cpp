#include <algorithm>
#include "i18n.hpp"
#include "util/dialog.hpp"
#include "widgets/text.hpp"
#include "widgets/button.hpp"
#include "eventbus.hpp"
#include <memory>

std::unique_ptr<Layout> make_dialog(int x, int y, int w, int h, std::string text) {
    auto layout = std::make_unique<Layout>(x, y, w, h, single_frame);
    auto content = std::make_unique<Text>(2, 1, w - 4, text);
    content->h = std::max(1, h - 5);
    layout->add(std::move(content));
    return layout;
}

std::unique_ptr<Layout> make_confirm_dialog(
    int x, int y, int w, int h,
    std::string text,
    std::function<void()> callback
) {
    auto layout = make_dialog(x, y, w, h, text);
    auto confirm = std::make_unique<Button>(10, h - 4, 20, 3, '\r', callback);
    confirm->set(std::make_unique<Text>(0, 0, 2, tr(msg::ActionConfirm {"Enter"})));
    layout->add(std::move(confirm));
    return layout;
}

std::unique_ptr<Layout> make_yesno_dialog(
    int x, int y, int w, int h,
    std::string text,
    std::function<void()> callback_yes,
    std::function<void()> callback_no
) {
    auto layout = make_dialog(x, y, w, h, text);
    auto yes = std::make_unique<Button>(22, h - 4, 16, 3, '\r', callback_yes);
    yes->set(std::make_unique<Text>(0, 0, 2, tr(msg::ActionYes {"Enter"})));
    layout->add(std::move(yes));
    auto no = std::make_unique<Button>(2, h - 4, 14, 3, Key::Escape, callback_no);
    no->set(std::make_unique<Text>(0, 0, 2, tr(msg::ActionNo {"Esc"})));
    layout->add(std::move(no));
    return layout;
}

void confirm(int w, int h, std::string text, std::function<void()> callback) {
    auto done = std::make_shared<bool>(false);

    auto layout = make_confirm_dialog(
        (80 - w) >> 1, (25 - h) >> 1, w, h, text,
        [callback = std::move(callback), done]() {
            if (*done) return;
            *done = true;
            callback();
            evbus->send(Events::IndexPop {});
        }
    );

    evbus->send(Events::IndexPush {std::move(layout)});
}

void yesno(int w, int h, std::string text, std::function<void()> callback_yes, std::function<void()> callback_no) {
    auto done = std::make_shared<bool>(false);

    auto layout = make_yesno_dialog(
        (80 - w) >> 1, (25 - h) >> 1, w, h, text,
        [callback = std::move(callback_yes), done]() {
            if (*done) return;
            *done = true;
            callback();
            evbus->send(Events::IndexPop {});
        },
        [callback = std::move(callback_no), done]() {
            if (*done) return;
            *done = true;
            callback();
            evbus->send(Events::IndexPop {});
        }
    );

    evbus->send(Events::IndexPush {std::move(layout)});
}
