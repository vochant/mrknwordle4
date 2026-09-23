#define NOMINMAX
#include "widgets/longmenu.hpp"
#include "i18n.hpp"
#include "options.hpp"
#include "services/mouse.service.hpp"
#include <algorithm>
#include <cmath>

namespace {
    int displayWidth(const std::string& text) {
        int width = 0;
        for (const auto& cluster : split_graphemes(text)) {
            width += term ? term->prepareGlyph(cluster).width : prepare_glyph(cluster).width;
        }
        return width;
    }
} // namespace

void LongMenu::render(PaintBrush* pb, bool redraw) {
    int hover = -1;
    if (options->term.mouse && mouse_service) {
        auto pos = mouse_service->where();
        const int num_visible = std::max(0, std::min(itemsPerPage, itemCount - page * itemsPerPage));
        if (pos.first >= ax && pos.first < ax + w &&
            pos.second >= ay && pos.second < ay + num_visible
        ) {
            hover = pos.second - ay;
        }
    }

    const auto prev = tr(Msg::PrevPage);
    const auto next = tr(Msg::NextPage);
    const auto indicator = tr(msg::Pagination {page + 1, numPages});
    const int prevWidth = std::min(w, displayWidth(prev));
    const int nextWidth = std::min(w, displayWidth(next));
    const int nextX = w - nextWidth;
    const int indicatorWidth = std::min(w, displayWidth(indicator));
    const int indicatorX = std::max(0, (w - indicatorWidth) / 2);
    bool prevHover = false, nextHover = false;
    if (options->term.mouse && mouse_service) {
        auto pos = mouse_service->where();
        const int localX = pos.first - ax;
        prevHover = page > 0 && pos.second == ay + h - 1 && localX >= 0 && localX < prevWidth;
        nextHover = page + 1 < numPages && pos.second == ay + h - 1 && localX >= nextX && localX < w;
    }

    if (page != prevPage || itemId != prevItem ||
        hover != prevHover ||
        prevHover != prevPrevHover ||
        nextHover != prevNextHover ||
        redraw
    ) {
        prevPage = page;
        prevItem = itemId;
        prevHover = hover;
        prevPrevHover = prevHover;
        prevNextHover = nextHover;

        auto items = getPage(page);
        int col = 0;
        for (const auto& item : items) {
            pb->locate(ax, ay + col);
            pb->color(
                col == itemId ?
                options->colors.selected : (
                    col == hover ?
                    options->colors.hover :
                    options->colors.foreground
                ),
                options->colors.background
            );
            pb->fill(ax, ay + col, ax + w - 1, ay + col);
            pb->locate(ax, ay + col);
            pb->textBox(item, w, 1, false);
            col++;
        }
        pb->color(options->colors.background, options->colors.background);
        for (int i = col; i < itemsPerPage; i++) {
            pb->locate(ax, ay + i);
            pb->text(std::string(w, ' '));
        }
        pb->color(options->colors.background, options->colors.background);
        pb->fill(ax, ay + h - 1, ax + w - 1, ay + h - 1);
        pb->locate(ax, ay + h - 1);
        pb->color(
            page ? (
                prevHover ?
                options->colors.hover :
                options->colors.foreground
            ) :
            options->colors.muted,
            options->colors.background
        );
        pb->textBox(prev, prevWidth, 1, false);
        pb->locate(ax + indicatorX, ay + h - 1);
        pb->color(options->colors.foreground, options->colors.background);
        pb->textBox(indicator, indicatorWidth, 1, false);
        pb->locate(ax + nextX, ay + h - 1);
        pb->color(
            page + 1 < numPages ? (
                nextHover ?
                options->colors.hover :
                options->colors.foreground
            ) :
            options->colors.muted,
            options->colors.background
        );
        pb->textBox(next, nextWidth, 1, false);
    }
}

bool LongMenu::onInput(int ch) {
    if (ch == '\r') {
        callback(page * itemsPerPage + itemId);
        return true;
    }

    if (ch == 'w' || ch == 'k' || ch == Key::Up) {
        itemId--;
        if (itemId < 0) {
            if (page) {
                page--;
                itemId = itemsPerPage - 1;
            }
            else itemId = 0;
        }
    }
    else if (ch == 's' || ch == 'j' || ch == Key::Down) {
        itemId++;
        if (itemId >= std::min(itemsPerPage, itemCount - page * itemsPerPage)) {
            if (page + 1 < numPages) {
                page++;
                itemId = 0;
            }
            else itemId--;
        }
    }
    else if (ch == 'a' || ch == 'h' || ch == Key::Left) {
        if (page) {
            page--;
            itemId = 0;
        }
    }
    else if (ch == 'd' || ch == 'l' || ch == Key::Right) {
        if (page + 1 < numPages) {
            page++;
            itemId = 0;
        }
    }
    else return false;
    return true;
}

void LongMenu::onClick(int ix, int iy) {
    if (ix < 0 || ix >= w || iy < 0 || iy >= h) return;
    int pageSize = std::min(itemsPerPage, itemCount - page * itemsPerPage);
    if (iy == h - 1) {
        const int prevWidth = std::min(w, displayWidth(tr(Msg::PrevPage)));
        const int nextWidth = std::min(w, displayWidth(tr(Msg::NextPage)));
        if (ix < prevWidth && page) {
            --page;
            itemId = 0;
        }
        else if (ix >= w - nextWidth && page + 1 < numPages) {
            ++page;
            itemId = 0;
        }
        return;
    }
    if (iy >= pageSize) return;
    callback(page * itemsPerPage + iy);
}

int LongMenu::getItemId() const { return page * itemsPerPage + itemId; }

void LongMenu::resetPtr() {
    page = 0;
    itemId = 0;
    prevPage = -1;
    prevItem = -1;
}

void LongMenu::setCount(int count) {
    itemCount = count;
    numPages = itemCount / itemsPerPage + !!(itemCount % itemsPerPage);
    if (page * itemsPerPage + itemId >= itemCount) {
        page = numPages - 1;
        if (itemCount * itemsPerPage == itemCount) itemId = itemsPerPage - 1;
        else itemId = itemCount % itemsPerPage - 1;
    }
}

LongMenu::LongMenu(
    int x, int y, int w, int h, int nItems,
    std::function<std::vector<std::string>(int)> getPage,
    std::function<void(int)> callback
) : Widget(x, y, w, h), getPage(getPage), callback(callback) {
    itemId = 0;
    page = 0;
    prevItem = -1;
    prevPage = -1;
    itemsPerPage = h - 1;
    itemCount = nItems;
    numPages = itemCount / itemsPerPage + !!(itemCount % itemsPerPage);
}
