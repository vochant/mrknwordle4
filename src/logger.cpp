#include "logger.hpp"
#include <ctime>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <system_error>
#include <utility>

using namespace std::chrono;

#ifndef WORDLE_LOG_LEVEL
#define WORDLE_LOG_LEVEL 1
#endif

static_assert(WORDLE_LOG_LEVEL >= Logger::Debug && WORDLE_LOG_LEVEL <= Logger::Error);
Logger logger(static_cast<Logger::Level>(WORDLE_LOG_LEVEL));

std::string get_time(char s0, char s1, char s2, char s3) {
    std::chrono::system_clock::time_point tp = std::chrono::system_clock::now();
    time_t t = std::chrono::system_clock::to_time_t(tp);
    tm storage {};
#ifdef _WIN32
    localtime_s(&storage, &t);
#else
    localtime_r(&t, &storage);
#endif
    tm* T = &storage;
    char str[128];
    auto stamp = tp.time_since_epoch();
    int ms = duration_cast<milliseconds>(stamp).count() - (1000 * duration_cast<seconds>(stamp).count());
    sprintf(
        str, "%04d%c%02d%c%02d%c%02d%c%02d%c%02d%c%03d",
        T->tm_year + 1900, s0, T->tm_mon + 1, s0, T->tm_mday, s2,
        T->tm_hour, s1, T->tm_min, s1, T->tm_sec, s3, ms
    );
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

void Logger::write_entry(const Entry& entry) {
    fs << '[' << entry.timestamp << "][" << entry.mod << '/' << level_string(entry.level) << "] " << entry.str << '\n';
    fs.flush();
}

void Logger::write(const Level l, std::string mod, std::string str) {
    if (l < level) return;
    std::lock_guard<std::mutex> guard(lock);
    Entry entry {get_time('-', ':', ' ', '.'), l, std::move(mod), std::move(str)};
    if (!started) {
        pending.push_back(std::move(entry));
        return;
    }
    if (fs.is_open()) write_entry(entry);
}

void Logger::start() {
    std::lock_guard<std::mutex> guard(lock);
    if (started) return;
    started = true;
    std::error_code error;
    std::filesystem::create_directories("logs", error);
    if (error) {
        pending.clear();
        return;
    }
    fs.open("logs/" + get_time('-', '-', '-', '-') + ".log", std::ios::out);
    if (!fs.is_open()) {
        pending.clear();
        return;
    }
    for (const auto& entry : pending) write_entry(entry);
    pending.clear();
}

Logger::Logger(const Level level) : level(level) {}

Logger::~Logger() { fs.close(); }

AutoLogger::AutoLogger(
    const Logger::Level level,
    Logger* logger,
    const std::string funcName
) : logger(logger), funcName(funcName), level(level) {
    logger->write(level, funcName, "进入函数");
}

AutoLogger::~AutoLogger() { logger->write(level, funcName, "退出函数"); }
