#include "services/mouse.service.hpp"

void MouseService::listen(std::function<void(int, int, bool)> listener) { listeners.push_back(std::move(listener)); }

void MouseService::dispatch(int x, int y, bool click, bool right) {
    mx = x;
    my = y;
    if (!click) return;
    auto cbs = listeners;
    for (const auto& cb : cbs) cb(x, y, right);
}

std::pair<int, int> MouseService::where() { return {mx, my}; }
