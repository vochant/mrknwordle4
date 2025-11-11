#include "services/mouse.service.hpp"
#include "global.h"

#include <thread>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "tick.hpp"
#include "logger.hpp"

namespace MouseServer {
	HWND hWnd;
	int *mx, *my;
	bool Shutdown, prevStateL, prevStateR;
	std::thread mth;
	std::vector<std::function<void(int, int, bool)>> *listeners;
	void boardcast(bool isRight) {
		for (auto& i : *listeners) {
			i(*mx, *my, isRight);
		}
	}
	void server() {
		AutoLogger autolog(Logger::Debug, &logger, "Mouse Server");
		while (!Shutdown) {
			if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
				if (!prevStateL) {
					boardcast(false);
					prevStateL = true;
				}
			}
			else prevStateL = false;
			if (GetAsyncKeyState(VK_RBUTTON) & 0x8000) {
				if (!prevStateR) {
					boardcast(true);
					prevStateR = true;
				}
			}
			else prevStateR = false;
			POINT pt;
			GetCursorPos(&pt);
			ScreenToClient(hWnd, &pt);
            COORD fontSize = GetConsoleFontSize(hOutput, 0);
            int font_w = fontSize.X;
            int font_h = fontSize.Y;
			*mx = pt.x / font_w;
			*my = pt.y / font_h;
			tick();
		}
	}
	void start(int* x, int* y, std::vector<std::function<void(int, int, bool)>>* l) {
		hWnd = GetForegroundWindow();
		mx = x;
		my = y;
		listeners = l;
		Shutdown = false;
		mth = std::thread(server);
	}
	void stop() {
		Shutdown = true;
		mth.join();
	}
}

void MouseService::listen(std::function<void(int, int, bool)> f) {
	listeners.push_back(f);
}

std::pair<int, int> MouseService::where() {
	return {mx, my};
}

MouseService::MouseService() {
	MouseServer::start(&mx, &my, &listeners);
}

MouseService::~MouseService() {
	MouseServer::stop();
}