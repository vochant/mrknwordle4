#pragma once

#include <vector>
#include <functional>

#include "widgets/widget.hpp"

class LongMenu : public Widget {
private:
    std::function<std::vector<std::string>(int)> getPage;
    std::function<void(int)> callback;
    int prevPage, prevItem, page, itemId, itemsPerPage, numPages, itemCount;

public:
    void render(PaintBrush* pb, bool redraw) override;
    void onInput(int ch) override;
    void onClick(int ix, int iy) override;
    int getItemId() const;
    void reload();
    void resetptr();
    void setcount(int count);

    LongMenu(int x, int y, int w, int h, int nItems, std::function<std::vector<std::string>(int)> getPage, std::function<void(int)> callback);
};