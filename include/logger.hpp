#pragma once

#include <string>
#include <fstream>
#include <mutex>

class Logger {
private:
	std::fstream fs;
    std::mutex lock;
public:
	enum Level {
		Debug, Info, Warn, Error
	} level;
	void write(Level l, std::string mod, std::string str);
	Logger(Level level);
	~Logger();
};

extern Logger logger;

class AutoLogger {
private:
	Logger* logger;
	std::string funcName;
	Logger::Level level;
public:
	AutoLogger(Logger::Level level, Logger* logger, std::string funcName);
	~AutoLogger();
};

#define AUTOLOG(LEVEL) AutoLogger __auto_log__(LEVEL, &logger, __func__)