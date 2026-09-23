#pragma once

#include <functional>
#include <utility>
#include <vector>

class MouseService {
private:
    int mx = -1, my = -1;
    std::vector<std::function<void(int, int, bool)>> listeners;

public:
    void dispatch(int x, int y, bool click, bool right);
    void listen(std::function<void(int, int, bool)> f);
    std::pair<int, int> where();
};

extern MouseService* mouse_service;
