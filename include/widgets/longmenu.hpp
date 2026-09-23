#pragma once

#include <vector>
#include <functional>

#include "widgets/widget.hpp"

class LongMenu : public Widget {
private:
    std::function<std::vector<std::string>(int)> getPage;
    std::function<void(int)> callback;
    int prevPage, prevItem, prevHover = -1, page, itemId, itemsPerPage, numPages, itemCount;
    bool prevPrevHover = false, prevNextHover = false;

public:
    void render(PaintBrush* pb, bool redraw) override;
    bool onInput(int ch) override;
    void onClick(int ix, int iy) override;
    int getItemId() const;
    void reload();
    void resetPtr();
    void setCount(int count);

    LongMenu(
        int x, int y, int w, int h, int nItems, std::function<std::vector<std::string>(int)> getPage,
        std::function<void(int)> callback
    );
};
