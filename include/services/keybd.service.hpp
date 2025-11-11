#pragma once

#include <functional>
#include <vector>

class KeyboardService {
private:
	std::vector<std::function<void(int)>> listeners;
public:
	bool isDown(int vk);
	void listen(std::function<void(int)> l);
	KeyboardService();
	~KeyboardService();
};

extern bool no_next;

extern KeyboardService* keybd_service;