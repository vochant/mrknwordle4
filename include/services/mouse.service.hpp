#pragma once

#include <functional>
#include <utility>
#include <vector>

class MouseService {
private:
	int mx, my;
	std::vector<std::function<void(int, int, bool)>> listeners;
public:
	void listen(std::function<void(int, int, bool)> f);
	std::pair<int, int> where();
	MouseService();
	~MouseService();
};

extern MouseService* mouse_service;