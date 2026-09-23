#pragma once

#include "widgets/widget.hpp"
#include <vector>
#include <memory>

class Index : public Widget {
private:
    std::vector<std::unique_ptr<Widget>> indexes;
    bool needRedraw = false;

public:
    template<class WidgetType>
    void push(std::unique_ptr<WidgetType> w) {
        push(std::unique_ptr<Widget>(std::move(w)));
    }
    void push(std::unique_ptr<Widget> w);
    void pop();
    void render(PaintBrush* pb, bool redraw) override;
    bool onInput(int ch) override;
    void onClick(int ix, int iy) override;
    void onWideClick() override;
    void setAbs(int x, int y) override;
    Index(int rx, int ry, int w, int h);
};
