#include "widgets/router.hpp"
#include "i18n.hpp"
#include "logger.hpp"
#include "options.hpp"

void Router::add(std::string name, std::unique_ptr<Widget> e) {
    e->setAbs(ax, ay);
    p.insert({std::move(name), std::move(e)});
}

void Router::route(std::string name) {
    if (name == current_name) return;

    if (name == "") current = nullptr;
    else if (p.count(name)) current = p.at(name).get();
    else {
        current = nullptr;
        logger.write(Logger::Warn, "Router Widget", "找不到路由路径 " + name);
    }
    current_name = name;
    needRedraw = true;
}

void Router::render(PaintBrush* pb, bool redraw) {
    if (redraw || needRedraw) {
        pb->color(options->colors.foreground, options->colors.background);
        pb->fill(ax, ay, ax + w - 1, ay + h - 1);
    }
    if (current) {
        current->render(pb, redraw || needRedraw);
        needRedraw = false;
    }
    else if (redraw || needRedraw) {
        pb->locate(ax + (w >> 1) - 4, ay + (h >> 1));
        pb->textBox(tr(Msg::HintNoScreen), w, h);
        needRedraw = false;
    }
}

bool Router::onInput(int ch) { return current && current->onInput(ch); }

void Router::onClick(int ix, int iy) {
    if (current) current->onClick(ix - current->rx, iy - current->ry);
}

void Router::onWideClick() {
    if (current) current->onWideClick();
}

void Router::setAbs(int x, int y) {
    ax = x + rx;
    ay = y + ry;
    for (auto& kv : p) kv.second->setAbs(ax, ay);
}

Router::Router(int rx, int ry, int w, int h) : Widget(rx, ry, w, h), current_name("") {
    needRedraw = false;
    p.clear();
    current = nullptr;
}
