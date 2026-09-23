#include "services/keybd.service.hpp"

void KeyboardService::setHandler(std::function<bool(int)> callback) { handler = std::move(callback); }

void KeyboardService::dispatch(int key) {
    if (handler) handler(key);
}
