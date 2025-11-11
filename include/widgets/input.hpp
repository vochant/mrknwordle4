#pragma once

#include "widgets/widget.hpp"
#include <functional>
#include <string>

bool acceptAll(char ch);
bool acceptNumber(char ch);
bool acceptIdentifier(char ch);

class Input : public Widget {
private:
    std::string content, placeholder;
    bool isActive, changed, isPassword, prevIsFocus, ready, nextFar;
    std::function<bool(char)> acceptCond;
    Input* next;

public:
    void render(PaintBrush* pb, bool redraw) override;
	void onInput(int k) override;
    void onClick(int ix, int iy) override;
    void onWideClick() override;
    void set(std::string str);
    std::string get();
    void setNext(Input* nxt, bool is_far);
public:
    Input(int rx, int ry, int w, bool isPassword = false, std::string content = "", std::string placeholder = "", std::function<bool(char)> acceptCond = acceptAll, bool initialActive = true);
};