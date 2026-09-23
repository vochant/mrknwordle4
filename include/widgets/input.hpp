#pragma once

#include "widgets/widget.hpp"
#include <functional>
#include <string>
#include <vector>

using InputFilter = std::function<bool(char32_t)>;

bool accept_all(char32_t ch);
bool accept_number(char32_t ch);
bool accept_identifier(char32_t ch);

class Input : public Widget {
private:
    std::vector<std::string> content;
    std::string placeholder;
    InputFilter filter;
    std::function<void()> submit;
    size_t cursor = 0, scroll = 0;
    bool active, password, focused = false, changed = true, prevHover = false;

    void insert(char32_t point);
    int glyphWidth(size_t index) const;

public:
    void render(PaintBrush* pb, bool redraw) override;
    bool onInput(int k) override;
    bool focusable() const override;
    void focus(bool active) override;
    void onClick(int ix, int iy) override;
    void onWideClick() override;
    void set(std::string str);
    std::string get() const;
    void setSubmit(std::function<void()> callback);

public:
    Input(
        int rx, int ry, int w, bool password = false, std::string content = "", std::string placeholder = "",
        InputFilter filter = accept_all, bool active = true
    );
};
