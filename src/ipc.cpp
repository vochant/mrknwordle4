#include "ipc.hpp"

#include "tick.hpp"
#include "logger.hpp"

void IPC::server() {
	AutoLogger autolog(Logger::Debug, &logger, "IPC Server");
	while (!Shutdown) {
		while (!q.empty()) {
			auto packet = q.front();
			q.pop();
			auto v = handlers.find(packet.type);
			if (v == handlers.end()) continue;
			for (auto& i : v->second) {
				i(packet);
			}
		}
		tick();
	}
}

void IPC::send(const Message msg) {
	q.push(msg);
}

void IPC::listen(std::string type, std::function<void(Message)> handler) {
	if (handlers.count(type)) {
		handlers.at(type).push_back(handler);
	}
	else {
		handlers.insert({type, {handler}});
	}
}

void IPC::reset(std::string type) {
    if (handlers.count(type)) {
        handlers.erase(type);
    }
}

IPC::IPC() {
	Shutdown = false;
	handlers.clear();
	mth = std::thread(std::mem_fn(&IPC::server), this);	
}

IPC::~IPC() {
	Shutdown = true;
	if (mth.joinable()) mth.join();
}