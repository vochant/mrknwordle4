#pragma once

#include "widgets/widget.hpp"

class Text : public Widget {
private:
    std::string text;
    Color f, b;
    bool needRedraw;
    int firstLine = 0, lineCount = 1;

public:
    std::string get();
    void set(std::string str, int len);
    virtual void render(PaintBrush* pb, bool redraw) override;
    void color(Color foreground);
    void color(Color foreground, Color background);
    bool onInput(int key) override;
    Text(int rx, int ry, int w, std::string str = "", Color f = 15, Color b = 0);
};
