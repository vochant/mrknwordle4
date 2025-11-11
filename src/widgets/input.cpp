#include "widgets/input.hpp"

#include <algorithm>
#include "render.hpp"

#include "services/keybd.service.hpp"

bool acceptAll(char ch) {
    return true;
}

bool acceptNumber(char ch) {
    return isdigit(ch);
}

bool acceptIdentifier(char ch) {
    return isalnum(ch) || ch == '_';
}

void Input::render(PaintBrush* pb, bool redraw) {
    redraw |= changed |= (isFocus() != prevIsFocus);
    changed = false;
    if (!redraw) return;
    prevIsFocus = isFocus();
    pb->color(prevIsFocus ? 14 : (isActive ? 10 : 15), 0);
    pb->rect(ax, ay, ax + w - 1, ay + 2, content == "");
    pb->locate(ax + 2, ay + 1);
    if (content == "") {
        pb->color(8);
        pb->text(placeholder);
        return;
    }
    int rw = w - 3;
    int offset = std::max<int>(content.length() - rw, 0);
    int length = std::min<int>(rw, content.length());
    pb->color(15);
    if (isPassword) for (int i = 0; i < length; i++) pb->single('*');
    else for (int i = 0; i < length; i++) pb->single(content[offset + i]);
    for (int i = length; i < rw; i++) pb->single(' ');
}

void Input::onInput(int k) {
    renderer_lock.lock();
    char kch = k;
    if (no_next) {
        renderer_lock.unlock();
        return;
    }
    if (kch == '\t') {
        if (isActive) {
            isActive = false;
            changed = true;
            if (nextFar) next->ready = true;
            else next->isActive = true, next->changed = true;
        }
        else if (ready) {
            ready = false;
            isActive = true;
            changed = true;
        }
        else {
            renderer_lock.unlock();
            return;
        }
        no_next = true;
    }
    else if (isActive && kch != '\r' && kch != VK_ESCAPE && k < 128) {
        if (kch == '\b') {
            content = content.substr(0, content.length() - 1);
            changed = true;
        }
        else if (acceptCond(kch)) {
            content += kch;
            changed = true;
        }
    }
    renderer_lock.unlock();
}

void Input::onClick(int ax, int ay) {
    renderer_lock.lock();
    if (!isActive) {
        isActive = true;
        changed = true;
    }
    renderer_lock.unlock();
}

void Input::onWideClick() {
    renderer_lock.lock();
    if (isActive) {
        isActive = false;
        changed = true;
    }
    ready = false;
    renderer_lock.unlock();
}

std::string Input::get() {
    return content;
}

void Input::set(std::string str) {
    renderer_lock.lock();
    content = str;
    changed = true;
    renderer_lock.unlock();
}

void Input::setNext(Input* nxt, bool is_far) {
    next = nxt;
    nextFar = is_far;
}

Input::Input(int rx, int ry, int w, bool isPassword, std::string content, std::string placeholder, std::function<bool(char)> acceptCond, bool initialActive) : Widget(rx, ry, w, 3), isPassword(isPassword), content(content), placeholder(placeholder), acceptCond(acceptCond), prevIsFocus(false), isActive(initialActive), next(this), nextFar(true), ready(false) {}