#pragma once

#include <functional>
class KeyboardService {
    std::function<bool(int)> handler;

public:
    void dispatch(int key);
    void setHandler(std::function<bool(int)> handler);
};

extern KeyboardService* keybd_service;
