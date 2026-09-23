#pragma once

#include "widgets/layout.hpp"
#include <functional>

std::unique_ptr<Layout> make_dialog(int x, int y, int w, int h,
    std::string text
);

std::unique_ptr<Layout> make_confirm_dialog(int x, int y, int w, int h,
    std::string text,
    std::function<void()> callback
);

std::unique_ptr<Layout> make_yesno_dialog(
    int x, int y, int w, int h,
    std::string text,
    std::function<void()> callback_yes, std::function<void()> callback_no
);

void confirm(int w, int h, std::string text, std::function<void()> callback);
void yesno(int w, int h, std::string text, std::function<void()> callback_yes, std::function<void()> callback_no);
