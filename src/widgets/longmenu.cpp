#define NOMINMAX
#include "widgets/longmenu.hpp"
#include "i18n.hpp"
#include "options.hpp"
#include "services/mouse.service.hpp"
#include <cmath>
#include "render.hpp"

void LongMenu::render(PaintBrush* pb, bool redraw) {
    if (page != prevPage || itemId != prevItem) redraw = true;
    if (redraw) {
        prevPage = page;
        prevItem = itemId;

        auto items = getPage(page);
        int col = 0;
        for (const auto& item : items) {
            pb->locate(ax, ay + col);
            pb->color(col == itemId ? 14 : 15, 0);
            pb->text(item);
            pb->color(0, 0);
            pb->text(std::string(w - item.length(), ' '));
            col++;
        }
        pb->color(0, 0);
        for (int i = col; i < itemsPerPage; i++) {
            pb->locate(ax, ay + i);
            pb->text(std::string(w, ' '));
        }
        pb->locate(ax, ay + h - 1);
        pb->color(page ? 15 : 8, 0);
        pb->text("<- PREV");
        pb->color(15, 0);
        pb->text(" [" + std::to_string(page + 1) + "/" + std::to_string(numPages) + "] ");
        pb->color((page + 1 < numPages) ? 15 : 8, 0);
        pb->text("NEXT ->");
    }
}

void LongMenu::onInput(int ch) {
    if (ch == '\r') {
        callback(page * itemsPerPage + itemId);
        return;
    }
    renderer_lock.lock();
    if (ch == 'w' || ch == 'k' || ch == 328) {
        itemId--;
        if (itemId < 0) {
            if (page) {
                page--;
                itemId = itemsPerPage - 1;
            }
            else itemId = 0;
        }
    }
    else if (ch == 's' || ch == 'j' || ch == 336) {
        itemId++;
        if (itemId >= std::min(itemsPerPage, itemCount - page * itemsPerPage)) {
            if (page + 1 < numPages) {
                page++;
                itemId = 0;
            }
            else itemId--;
        }
    }
    else if (ch == 'a' || ch == 'h' || ch == 331) {
        if (page) {
            page--;
            itemId = 0;
        }
    }
    else if (ch == 'd' || ch == 'l' || ch == 333) {
        if (page + 1 < numPages) {
            page++;
            itemId = 0;
        }
    }
    renderer_lock.unlock();
}

void LongMenu::onClick(int ix, int iy) {
    if (ix < 0 || ix > w || iy < 0 || iy >= h) return;
    int pageSize = std::min(itemsPerPage, itemCount - page * itemsPerPage);
    if (iy == h - 1) {
        auto[x, y] = mouse_service->where();
        int offset = 12 + floor(log10(page + 1)) + floor(log10(numPages));
        if (y == ay + h - 1 && x >= ax && x <= ax + 6 && page) page--, itemId = 0; 
        if (y == ay + h - 1 && x >= ax + offset && x <= ax + offset + 6 && page + 1 < numPages) page++, itemId = 0;
        return;
    }
    if (iy >= pageSize) return;
    callback(page * pageSize + iy);
}

int LongMenu::getItemId() const {
    return page * itemsPerPage + itemId;
}

void LongMenu::resetptr() {
    renderer_lock.lock();
    page = 0;
    itemId = 0;
    prevPage = -1;
    prevItem = -1;
    renderer_lock.unlock();
}

void LongMenu::setcount(int count) {
    renderer_lock.lock();
    itemCount = count;
    numPages = itemCount / itemsPerPage + !!(itemCount % itemsPerPage);
    if (page * itemsPerPage + itemId >= itemCount) {
        page = numPages - 1;
        if (itemCount * itemsPerPage == itemCount) {
            itemId = itemsPerPage - 1;
        }
        else {
            itemId = itemCount % itemsPerPage - 1;
        }
    }
    renderer_lock.unlock();
}

LongMenu::LongMenu(int x, int y, int w, int h, int nItems, std::function<std::vector<std::string>(int)> getPage, std::function<void(int)> callback) : Widget(x, y, w, h), getPage(getPage), callback(callback) {
    itemId = 0;
    page = 0;
    prevItem = -1;
    prevPage = -1;
    itemsPerPage = h - 1;
    itemCount = nItems;
    numPages = itemCount / itemsPerPage + !!(itemCount % itemsPerPage);
}