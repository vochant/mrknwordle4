#pragma once

#include "widgets/layout.hpp"
#include <functional>

std::shared_ptr<Layout> makeDialog(int x, int y, int w, int h, std::string text);
std::shared_ptr<Layout> makeConfirmDialog(int x, int y, int w, int h, std::string text, std::function<void()> callback);
std::shared_ptr<Layout> makeYesNoDialog(int x, int y, int w, int h, std::string text, std::function<void()> callback_yes, std::function<void()> callback_no);
void confirm(int w, int h, std::string text, std::function<void()> callback);
void yesno(int w, int h, std::string text, std::function<void()> callback_yes, std::function<void()> callback_no);