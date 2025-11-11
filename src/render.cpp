#include "render.hpp"

#include <queue>
#include <ctime>
#include "ipc.hpp"
#include "logger.hpp"
#include "tick.hpp"

float fps;

Widget* root_widget;

std::mutex renderer_lock;

namespace Renderer {
	std::queue<clock_t> fps_q;

	void update_fps() {
		clock_t now = clock();
		fps_q.push(now);

		while (fps_q.size() > 1600 || now - fps_q.front() > 5000) fps_q.pop();

		if (now - fps_q.front() < 10) fps = 0;
		else if (fps_q.size() >= 1600) fps = -1;
		else fps = fps_q.size() * 1000.0 / (now - fps_q.front());
	}
}

void renderer(PaintBrush* pb) {
	AUTOLOG(Logger::Info);
	
	fps = -1;

	bool Shutdown = false;

	ipc->listen("shutdown", [&Shutdown](Message msg) {
		Shutdown = true;
	});

	bool needRedraw = false;

	Renderer::fps_q.push(clock());

	root_widget->render(pb, true);

	while (!Shutdown) {
		renderer_lock.lock();
		Renderer::update_fps();
		root_widget->render(pb, needRedraw);
		needRedraw = false;
		renderer_lock.unlock();
	}
}