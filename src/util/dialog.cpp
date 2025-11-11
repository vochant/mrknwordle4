#include "util/dialog.hpp"
#include "widgets/text.hpp"
#include "widgets/button.hpp"
#include "ipc.hpp"
#include <memory>
#include <atomic>

std::shared_ptr<Layout> makeDialog(int x, int y, int w, int h, std::string text) {
    auto layout = std::make_shared<Layout>(x, y, w, h, single_frame);
    layout->add(std::make_shared<Text>(2, 1, w - 3, text));
    return layout;
}

std::shared_ptr<Layout> makeConfirmDialog(int x, int y, int w, int h, std::string text, std::function<void()> callback) {
    auto layout = makeDialog(x, y, w, h, text);
    auto confirm = std::make_shared<Button>((w - 13) >> 1, h - 4, 13, 3, '\r', callback);
    confirm->set(std::make_shared<Text>(0, 0, 2, "OK (Enter)"));
    layout->add(confirm);
    return layout;
}

std::shared_ptr<Layout> makeYesNoDialog(int x, int y, int w, int h, std::string text, std::function<void()> callback_yes, std::function<void()> callback_no) {
    auto layout = makeDialog(x, y, w, h, text);
    auto yes = std::make_shared<Button>(w - 15, h - 4, 14, 3, '\r', callback_yes);
    yes->set(std::make_shared<Text>(0, 0, 2, "YES (Enter)"));
    layout->add(yes);
    auto no = std::make_shared<Button>(1, h - 4, 11, 3, VK_ESCAPE, callback_no);
    no->set(std::make_shared<Text>(0, 0, 2, "NO (Esc)"));
    layout->add(no);
    return layout;
}

void confirm(int w, int h, std::string text, std::function<void()> callback) {
    auto cb_ptr = std::make_shared<std::function<void()>>(std::move(callback));
    auto done = std::make_shared<std::atomic<bool>>(false);

    auto layout = makeConfirmDialog((80 - w) >> 1, (25 - h) >> 1, w, h, text, [cb_ptr, done]() {
        if (done->exchange(true)) return;
        (*cb_ptr)();
        ipc->send({"index-pop", nullptr});
    });

    ipc->send({"index-push", layout});
}

void yesno(int w, int h, std::string text, std::function<void()> callback_yes, std::function<void()> callback_no) {
    auto cb_yes = std::make_shared<std::function<void()>>(std::move(callback_yes));
    auto cb_no = std::make_shared<std::function<void()>>(std::move(callback_no));
    auto done = std::make_shared<std::atomic<bool>>(false);

    auto layout = makeYesNoDialog((80 - w) >> 1, (25 - h) >> 1, w, h, text, [cb_yes, done]() {
        if (done->exchange(true)) return;
        (*cb_yes)();
        ipc->send({"index-pop", nullptr});
    }, [cb_no, done]() {
        if (done->exchange(true)) return;
        (*cb_no)();
        ipc->send({"index-pop", nullptr});
    });

    ipc->send({"index-push", layout});
}