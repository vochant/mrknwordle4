#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <vector>

class Logger {
public:
    enum Level { Debug, Info, Warn, Error } level;

private:
    struct Entry {
        std::string timestamp;
        Level level;
        std::string mod;
        std::string str;
    };

    std::fstream fs;
    std::mutex lock;
    std::vector<Entry> pending;
    bool started = false;

    void write_entry(const Entry& entry);

public:
    void write(Level l, std::string mod, std::string str);
    void start();
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
