#pragma once

#include "widgets/widget.hpp"
#include <vector>
#include <memory>
#include <functional>

void no_frame(int x, int y, int w, int h, PaintBrush* pb);
void single_frame(int x, int y, int w, int h, PaintBrush* pb);

class Layout : public Widget {
public:
    typedef std::function<void(int, int, int, int, PaintBrush*)> FrameDrawer;

private:
    std::vector<std::unique_ptr<Widget>> items;
    FrameDrawer fd;
    size_t focused = 0;
    bool hasFocused = false;
    std::function<bool(int)> inputHandler;

    void focusNext();
    void focusPrev();
    void clearFocus();

public:
    void render(PaintBrush* pb, bool redraw) override;
    template<class WidgetType>
    void add(std::unique_ptr<WidgetType> w) {
        add(std::unique_ptr<Widget>(std::move(w)));
    }
    void add(std::unique_ptr<Widget> w);
    void setInput(std::function<bool(int)> handler);
    bool onInput(int ch) override;
    void onClick(int ix, int iy) override;
    void onWideClick() override;
    void setAbs(int x, int y) override;

public:
    Layout(int rx, int ry, int w, int h, FrameDrawer fd);
};
