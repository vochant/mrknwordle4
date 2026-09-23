#include "render.hpp"
#include "i18n.hpp"
#include "eventbus.hpp"
#include "logger.hpp"
#include "options.hpp"
#include "services/keybd.service.hpp"
#include "services/mouse.service.hpp"
#include "terminal/terminal.hpp"

#include <chrono>

float fps = 0;
Widget* root_widget = nullptr;

using namespace std::chrono;

void renderer(PaintBrush* brush) {
    AUTOLOG(Logger::Info);
    bool shutdown = false;
    evbus->listen<Events::Shutdown>([&shutdown](const Events::Shutdown&) { shutdown = true; });
    struct Subscription {
        ~Subscription() { evbus->reset<Events::Shutdown>(); }
    } subscription;
    bool redraw = true;
    auto prevFrame = steady_clock::now();
    auto nextFrame = prevFrame;
    while (!shutdown && !term_interrupted()) {
        evbus->dispatch();
        if (shutdown) break;
        auto now = steady_clock::now();
        if (now >= nextFrame) {
            auto dims = term->size();
            if (dims.first < 80 || dims.second < 25) {
                if (redraw) {
                    brush->color(options->colors.foreground, options->colors.background);
                    brush->fill(0, 0, dims.first - 1, dims.second - 1);
                    brush->locate(0, 0);
                    brush->textBox(tr(msg::TermResize {80, 25}), dims.first, dims.second);
                }
            }
            else if (root_widget) { root_widget->render(brush, redraw); }
            termScreen.present(*term);
            redraw = false;
            auto elapsed = std::chrono::duration<float>(now - prevFrame).count();
            fps = elapsed > 0 ? 1.0f / elapsed : 0;
            prevFrame = now;
            nextFrame = now + std::chrono::milliseconds(33);
        }
        int wait = duration_cast<milliseconds>(nextFrame -steady_clock::now()).count();
        auto event = term->poll(wait >= 0 ? wait + 1 : 0);
        if (event.type == TermEvent::Type::Close) break;
        if (event.type == TermEvent::Type::Resize) {
            auto dims = term->size();
            termScreen.resize(dims.first, dims.second);
            termScreen.invalidate();
            redraw = true;
        }
        else if (event.type == TermEvent::Type::Key) {
            if (event.key == 3 && options->term.handleInterrupt) break;
            keybd_service->dispatch(event.key);
        }
        else if (event.type == TermEvent::Type::Mouse && mouse_service) {
            mouse_service->dispatch(event.x, event.y, event.click, event.right);
        }
    }
}
