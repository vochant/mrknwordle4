#include "eventbus.hpp"

#include <stdexcept>

bool EventBus::send(Event event) {
    std::lock_guard<std::mutex> guard(mutex);
    if (closed) return false;
    queue.push(std::move(event));
    return true;
}

size_t EventBus::dispatch(size_t limit) {
    if (std::this_thread::get_id() != owner) throw std::logic_error("Events must be dispatched on the UI thread");
    size_t count = 0;
    while (count < limit) {
        Event event;
        std::vector<Handler> cbs;
        {
            std::lock_guard<std::mutex> guard(mutex);
            if (closed || queue.empty()) break;
            event = std::move(queue.front());
            queue.pop();
            cbs = handlers[event.index()];
        }
        for (const auto& cb : cbs) cb(event);
        count++;
    }
    return count;
}

void EventBus::close() {
    std::queue<Event> discarded;
    Handlers removed;
    {
        std::lock_guard<std::mutex> guard(mutex);
        closed = true;
        queue.swap(discarded);
        handlers.swap(removed);
    }
}

EventBus::~EventBus() { close(); }
