#include "userdb.hpp"
#include "bcrypt.h"
#include "logger.hpp"
#include <ctime>
#include <unicode/datefmt.h>
#include <unicode/unistr.h>
#include <unicode/calendar.h>
#include <sstream>
#include "i18n.hpp"

sqlite3* user_db;
int uid;
std::string username, search_engine;
std::vector<int> history_ids;
icu_74::DateFormat* userdb_fmt, * userdb_fmt_long;

std::string time_to_string(long long time, bool _long = false) {
    UDate unow = static_cast<UDate>(time) * 1000.0;
    
    icu::UnicodeString result;
    if (!_long) userdb_fmt->format(unow, result);
    else userdb_fmt_long->format(unow, result);

    result.findAndReplace(u"\u202F", u" ");

    std::string utf8_result;
    result.toUTF8String(utf8_result);
    return utf8_result;
}

bool init_user_db() {
    userdb_fmt = icu::DateFormat::createDateTimeInstance(
        icu::DateFormat::SHORT,
        icu::DateFormat::DEFAULT,
        icu::Locale((options->language + (options->calendar != "default" ? "@calendar=" + options->calendar : "")).c_str())
    );

    userdb_fmt_long = icu::DateFormat::createDateTimeInstance(
        icu::DateFormat::FULL,
        icu::DateFormat::MEDIUM,
        icu::Locale((options->language + (options->calendar != "default" ? "@calendar=" + options->calendar : "")).c_str())
    );

    if (options->dictSearch) search_engine = options->dictSearchEngine;
    uid = -1;
    username = "";
    int rc = sqlite3_open("user.db", &user_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(user_db));
        logger.write(Logger::Error, "userdb", std::string("无法打开数据库: ") + sqlite3_errmsg(user_db));
        return false;
    }
    if (options->walType > 0) {
        rc = sqlite3_exec(user_db, "PRAGMA journal_mode=WAL;", 0, 0, 0);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to set WAL mode: %s\n", sqlite3_errmsg(user_db));
            logger.write(Logger::Error, "userdb", std::string("无法设置 WAL 模式: ") + sqlite3_errmsg(user_db));
        }
    }
    rc = sqlite3_exec(user_db, "PRAGMA foreign_keys=ON;", 0, 0, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to enable foreign keys: %s\n", sqlite3_errmsg(user_db));
        logger.write(Logger::Error, "userdb", std::string("无法启用外键: ") + sqlite3_errmsg(user_db));
    }
    rc = sqlite3_exec(user_db, "CREATE TABLE IF NOT EXISTS users (uid INTEGER PRIMARY KEY, username TEXT UNIQUE, password TEXT, search_engine TEXT);", 0, 0, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create users table: %s\n", sqlite3_errmsg(user_db));
        logger.write(Logger::Error, "userdb", std::string("无法创建用户表: ") + sqlite3_errmsg(user_db));
        return false;
    }
    rc = sqlite3_exec(user_db, "CREATE TABLE IF NOT EXISTS histories (id INTEGER PRIMARY KEY, uid INTEGER NOT NULL, answer TEXT NOT NULL, history TEXT NOT NULL, gamemode TEXT NOT NULL, num_guess INTEGER NOT NULL, timestamp INTEGER NOT NULL, FOREIGN KEY(uid) REFERENCES users(uid) ON DELETE CASCADE);", 0, 0, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create histories table: %s\n", sqlite3_errmsg(user_db));
        logger.write(Logger::Error, "userdb", std::string("无法创建历史记录表: ") + sqlite3_errmsg(user_db));
        return false;
    }
    rc = sqlite3_exec(user_db, "CREATE INDEX IF NOT EXISTS idx_histories_uid_id ON histories(uid, id ASC);", 0, 0, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create index on histories table: %s\n", sqlite3_errmsg(user_db));
        logger.write(Logger::Error, "userdb", std::string("无法创建历史记录表索引: ") + sqlite3_errmsg(user_db));
        return false;
    }
    return true;
}

void close_user_db() {
    if (!user_db) return;
    
    if (options->walType == 1) {
        logger.write(Logger::Debug, "userdb", "执行 WAL 检查点以合并数据");
        
        // 首先尝试标准的 FULL 检查点
        int rc = sqlite3_wal_checkpoint_v2(user_db, nullptr, SQLITE_CHECKPOINT_FULL, nullptr, nullptr);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "WAL checkpoint failed: %s\n", sqlite3_errmsg(user_db));
            logger.write(Logger::Error, "userdb", std::string("WAL 检查点失败: ") + sqlite3_errmsg(user_db));
        }
    }
    sqlite3_close(user_db);
    user_db = nullptr;
}

// return values:
// 0 : successful
// 1 : system error
// 2 : not allowed operation

int login_user(std::string username, std::string password) {
    if (user_db == nullptr) return 1;

    std::string sql = "SELECT uid, password, search_engine FROM users WHERE username = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return 1;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        std::string hashed_password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (bcrypt::validatePassword(password, hashed_password)) {
            uid = sqlite3_column_int(stmt, 0);
            search_engine = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            ::username = username;
            sqlite3_finalize(stmt);
            return 0;
        }
    }
    sqlite3_finalize(stmt);
    return 2;
}

int create_user(std::string username, std::string password) {
    if (user_db == nullptr) return 1;

    std::string hashed_password = bcrypt::generateHash(password);
    std::string precheck = "SELECT 0 FROM users WHERE username = ?";
    std::string sql = "INSERT INTO users (username, password, search_engine) VALUES (?, ?, ?)";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, precheck.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return 1;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return 2;
    }
    sqlite3_finalize(stmt);
    rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return 1;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, hashed_password.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, options->dictSearchEngine.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        logger.write(Logger::Error, "userdb", std::string("无法创建用户: ") + sqlite3_errmsg(user_db));
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);
    return login_user(username, password);
}

bool check_password(std::string password) {
    if (user_db == nullptr) return false;

    std::string sql = "SELECT password FROM users WHERE uid = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return false;
    }
    sqlite3_bind_int(stmt, 1, uid);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        std::string hashed_password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        return bcrypt::validatePassword(password, hashed_password);
    }
    sqlite3_finalize(stmt);
    return false;
}

bool change_password(std::string password) {
    if (user_db == nullptr) return false;

    std::string sql = "UPDATE users SET password = ? WHERE uid = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return false;
    }
    std::string hashed_password = bcrypt::generateHash(password);
    sqlite3_bind_text(stmt, 1, hashed_password.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, uid);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        logger.write(Logger::Error, "userdb", std::string("无法更改密码: ") + sqlite3_errmsg(user_db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

bool change_username(std::string username) {
    if (user_db == nullptr) return false;

    std::string sql = "UPDATE users SET username = ? WHERE uid = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return false;
    }
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, uid);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        logger.write(Logger::Error, "userdb", std::string("无法更改用户名: ") + sqlite3_errmsg(user_db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    ::username = username;
    return true;
}

bool change_search_engine(std::string search_engine) {
    if (user_db == nullptr) return false;

    std::string sql = "UPDATE users SET search_engine = ? WHERE uid = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return false;
    }
    sqlite3_bind_text(stmt, 1, search_engine.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, uid);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        logger.write(Logger::Error, "userdb", std::string("无法更改搜索引擎: ") + sqlite3_errmsg(user_db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    ::search_engine = search_engine;
    return true;
}

bool write_history(std::string answer, std::string history, int num_guess, std::string gamemode) {
    if (user_db == nullptr) return false;
    if (uid == -1) return true;

    long long current_time = std::time(nullptr);

    std::string sql = "INSERT INTO histories (uid, answer, history, num_guess, gamemode, timestamp) VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return false;
    }
    sqlite3_bind_int(stmt, 1, uid);
    sqlite3_bind_text(stmt, 2, answer.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, history.c_str(), history.size(), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 4, num_guess);
    sqlite3_bind_text(stmt, 5, gamemode.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 6, current_time);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        logger.write(Logger::Error, "userdb", std::string("无法写入历史记录: ") + sqlite3_errmsg(user_db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    return true;
}

void logout_user() {
    uid = -1;
    username = "";
    search_engine = options->dictSearchEngine;
}

bool remove_user() {
    if (user_db == nullptr) return false;

    std::string sql = "DELETE FROM users WHERE uid = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return false;
    }
    sqlite3_bind_int(stmt, 1, uid);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        logger.write(Logger::Error, "userdb", std::string("无法删除用户: ") + sqlite3_errmsg(user_db));
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);
    logout_user();
    return true;
}

int count_history() {
    if (user_db == nullptr) return 0;
    if (uid == -1) return 0;

    std::string sql = "SELECT COUNT(*) FROM histories WHERE uid = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return 0;
    }
    sqlite3_bind_int(stmt, 1, uid);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        logger.write(Logger::Error, "userdb", std::string("无法获取历史记录数量: ") + sqlite3_errmsg(user_db));
        sqlite3_finalize(stmt);
        return 0;
    }
    int count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return count;
}

std::vector<std::string> get_history(int offset, int limit) {
    if (user_db == nullptr || uid == -1) return {};

    std::vector<std::string> result;
    std::string sql = "SELECT id, answer, timestamp FROM histories WHERE uid = ? ORDER BY timestamp DESC LIMIT ? OFFSET ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return {};
    }
    sqlite3_bind_int(stmt, 1, uid);
    sqlite3_bind_int(stmt, 2, limit);
    sqlite3_bind_int(stmt, 3, offset);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        if (rc == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return {};
        }
        logger.write(Logger::Error, "userdb", std::string("无法获取历史记录: ") + sqlite3_errmsg(user_db));
        sqlite3_finalize(stmt);
        return {};
    }
    history_ids.clear();
    while (rc == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char* answer = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        long long timestamp = sqlite3_column_int64(stmt, 2);
        result.push_back("[" + std::to_string(id) + "] " + time_to_string(timestamp) + " " + answer);
        history_ids.push_back(id);
        rc = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::string get_history_detail(int id) {
    if (user_db == nullptr) return translate("{hint.invalid_history}");

    std::string sql = "SELECT answer, history, num_guess, gamemode, timestamp FROM histories WHERE id = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return translate("{hint.invalid_history}");
    }
    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        if (rc == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return translate("{hint.invalid_history}");
        }
        logger.write(Logger::Error, "userdb", std::string("无法获取历史记录: ") + sqlite3_errmsg(user_db));
        sqlite3_finalize(stmt);
        return translate("{hint.invalid_history}");
    }
    std::stringstream ss;
    ss << '#' << id << '\n';
    ss << translate("{slot.playtime}: ") << time_to_string(sqlite3_column_int64(stmt, 4), true) << '\n';
    ss << translate("{slot.gamemode}: ") << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)) << '\n';
    ss << translate("{slot.answer}: ") << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)) << '\n';
    int num_guess = sqlite3_column_int(stmt, 2);
    if (~num_guess) {
        std::string hint_num_guess = translate("{slot.num_guess}");
        int log10_num_guess = num_guess ? std::floor(std::log10(num_guess)) : 1;
        char* result = new char[hint_num_guess.length() + log10_num_guess];
        std::sprintf(result, hint_num_guess.c_str(), num_guess);
        ss << result << '\n';
        delete[] result;
    }
    else ss << translate("{hint.failed_to_guess}") << '\n';
    ss << reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)) << '\n';
    sqlite3_finalize(stmt);
    return ss.str();
}