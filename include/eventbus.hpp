#pragma once

#include <functional>
#include <array>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <variant>
#include <vector>
#include "widgets/layout.hpp"

namespace Events {
    struct Shutdown {};
    struct Main {};
    struct IndexPop {};
    struct IndexPush {
        std::unique_ptr<Layout> layout;
    };
    struct Route {
        std::string name;
    };
    struct GameStart {
        std::string grader, dictionary;
        bool validate = true, answerOnly = true, showAlphabet = true;
        int maxGuesses = -1;
    };
    struct PluginLoad {
        std::string text;
    };
    struct LoadHistory {
        int id;
    };
    struct LoginClear {};
    struct RegisterClear {};
    struct UpdateUser {};
    struct SecurityClear {};
    struct DeleteClear {};
    struct SearchEngineChanged {};
    struct UpdateHistory {};
    struct UpdateDict {};
    struct ResetFilter {};
    struct UpdateFilterText {};
    struct UpdateWord {};
} // namespace Events

using Event = std::variant<
    Events::Shutdown, Events::Main, Events::IndexPop, Events::IndexPush, Events::Route, Events::GameStart,
    Events::PluginLoad, Events::LoadHistory, Events::LoginClear, Events::RegisterClear, Events::UpdateUser,
    Events::SecurityClear, Events::DeleteClear, Events::SearchEngineChanged, Events::UpdateHistory, Events::UpdateDict,
    Events::ResetFilter, Events::UpdateFilterText, Events::UpdateWord>;

class EventBus {
    using Handler = std::function<void(Event&)>;
    using Handlers = std::array<std::vector<Handler>, std::variant_size_v<Event>>;
    std::mutex mutex;
    std::queue<Event> queue;
    Handlers handlers;
    const std::thread::id owner = std::this_thread::get_id();
    bool closed = false;

public:
    bool send(Event event);
    size_t dispatch(size_t limit = 256);
    void close();
    template<class EventType, class Callback>
    void listen(Callback callback) {
        std::lock_guard<std::mutex> guard(mutex);
        if (closed) return;
        handlers[Event {EventType {}}.index()].push_back([callback = std::move(callback)](Event& event) {
            callback(std::get<EventType>(event));
        });
    }
    template<class EventType>
    void reset() {
        std::vector<Handler> removed;
        {
            std::lock_guard<std::mutex> guard(mutex);
            removed.swap(handlers[Event {EventType {}}.index()]);
        }
    }
    ~EventBus();
};

extern EventBus* evbus;
