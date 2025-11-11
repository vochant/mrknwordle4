#include "services/keybd.service.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <conio.h>
#include <thread>
#include "tick.hpp"
#include "logger.hpp"

namespace KeyboardServer {
	std::vector<std::function<void(int)>>* listeners;
	bool Shutdown, extra;
	std::thread mth;
	void boardcast(int vk) {
        if (vk == 224) {
            extra = true;
            return;
        }
        no_next = false;
        if (extra) vk += 256;
        extra = false;
		for (auto& i : *listeners) {
			i(vk);
		}
	}
	void server() {
        extra = false;
		AutoLogger autolog(Logger::Debug, &logger, "Keyboard Server");
		while (!Shutdown) {
			if (kbhit()) boardcast(getch());
			tick();
		}
	}
	void start(std::vector<std::function<void(int)>>* l) {
		listeners = l;
		Shutdown = false;
		mth = std::thread(server);
	}
	void stop() {
		Shutdown = true;
		mth.join();
	}
}

bool KeyboardService::isDown(int vk) {
	return GetAsyncKeyState(vk) & 0x8000;
}

void KeyboardService::listen(std::function<void(int)> l) {
	listeners.push_back(l);
}

KeyboardService::KeyboardService() {
	KeyboardServer::start(&listeners);
}

KeyboardService::~KeyboardService() {
	KeyboardServer::stop();
}

bool no_next = false;