#pragma once

#include <queue>
#include <string>
#include <any>
#include <functional>
#include <map>
#include <vector>
#include <thread>
#include <mutex>

struct Message {
	std::string type;
	std::any payload;
};

class IPC {
private:
	std::thread mth;
	std::queue<Message> q;
	bool Shutdown;
	typedef std::vector<std::function<void(Message)>> handlerset;
	std::map<std::string, handlerset> handlers;
public:
	void server();
	void send(const Message msg);
	void listen(std::string type, std::function<void(Message)> handler);
    void reset(std::string type);
	IPC();
	~IPC();
};

extern IPC* ipc;