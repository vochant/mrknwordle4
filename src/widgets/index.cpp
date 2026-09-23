#include "widgets/index.hpp"

void Index::push(std::unique_ptr<Widget> w) {
    w->setAbs(ax, ay);
    indexes.push_back(std::move(w));
    needRedraw = true;
}

void Index::pop() {
    if (!indexes.empty()) indexes.pop_back();
    needRedraw = true;
}

void Index::render(PaintBrush* pb, bool redraw) {
    if (redraw || needRedraw) {
        for (auto& i : indexes) { i->render(pb, true); }
        needRedraw = false;
    }
    else {
        if (indexes.size()) {
            indexes[indexes.size() - 1]->render(pb, false);
        }
    }
}

bool Index::onInput(int ch) { return indexes.size() && indexes[indexes.size() - 1]->onInput(ch); }

void Index::onClick(int ix, int iy) {
    if (indexes.size()) {
        auto& i = indexes[indexes.size() - 1];
        i->onClick(ix - i->rx, iy - i->ry);
    }
}

void Index::onWideClick() {
    if (indexes.size()) {
        auto& i = indexes[indexes.size() - 1];
        i->onWideClick();
    }
}

void Index::setAbs(int x, int y) {
    ax = x + rx;
    ay = y + ry;
    for (auto& i : indexes) { i->setAbs(ax, ay); }
}

Index::Index(int rx, int ry, int w, int h) : Widget(rx, ry, w, h), needRedraw(true) {}
