#include "logger.hpp"
#include <ctime>
#include <chrono>
#include <cstdio>
#include <filesystem>

std::string get_time(char s0, char s1, char s2, char s3) {
	std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
	time_t t = std::chrono::system_clock::to_time_t(tp);
	tm* T = localtime(&t);
	char str[128];
	int ms = std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count() - (1000 * std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count());
	sprintf(str, "%04d%c%02d%c%02d%c%02d%c%02d%c%02d%c%03d", T->tm_year + 1900, s0, T->tm_mon + 1, s0, T->tm_mday, s2, T->tm_hour, s1, T->tm_min, s1, T->tm_sec, s3, ms);
	return str;
}

std::string level_string(Logger::Level l) {
	switch (l) {
	case Logger::Error:
		return "ERROR";
	case Logger::Warn:
		return "WARN";
	case Logger::Info:
		return "INFO";
	case Logger::Debug:
		return "DEBUG";
	default:
		return "UNKNOWN";
	}
}

void Logger::write(const Level l, const std::string mod, const std::string str) {
    if (l < level) return;
    lock.lock();
	fs << '[' << get_time('-', ':', ' ', '.') << "][" << mod << '/' << level_string(l) <<  "] " << str << '\n';
	fs.flush();
    lock.unlock();
}

Logger::Logger(const Level level) : level(level) {
    std::filesystem::create_directories("logs");
	fs.open("logs/" + get_time('-', '-', '-', '-') + ".log", std::ios::out);
}

Logger::~Logger() {
	fs.close();
}

AutoLogger::AutoLogger(const Logger::Level level, Logger* logger, const std::string funcName) : level(level), logger(logger), funcName(funcName) {
	logger->write(level, funcName, "进入函数");
}

AutoLogger::~AutoLogger() {
	logger->write(level, funcName, "退出函数");
}