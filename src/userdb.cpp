#include "userdb.hpp"
#include <cmath>
#include "bcrypt.h"
#include "logger.hpp"
#include <ctime>
#include <sstream>
#include "i18n.hpp"
#include "registry.hpp"

sqlite3* user_db;
int uid;
std::string username, search_engine;
std::vector<int> history_ids;
std::string time_to_string(long long time, bool detailed = false) { return I18n::active().dateTime(time, detailed); }

namespace {
    std::string column_text(sqlite3_stmt* stmt, int column) {
        const auto* value = (const char*) sqlite3_column_text(stmt, column);
        return value ? value : "";
    }

    std::string dict_display_name(const std::string& id) {
        if (registry) {
            const auto found = registry->dicts.find(id);
            if (found != registry->dicts.end()) return found->second.name;
        }
        return id;
    }

    std::string grader_display_name(const std::string& id) {
        if (registry) {
            const auto found = registry->graders.find(id);
            if (found != registry->graders.end()) return found->second->name;
        }
        return id;
    }

} // namespace

bool init_user_db() {
    if (options->dict.search) search_engine = options->dict.searchEngine;
    uid = -1;
    username = "";
    int rc = sqlite3_open("user.db", &user_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(user_db));
        logger.write(Logger::Error, "userdb", std::string("无法打开数据库: ") + sqlite3_errmsg(user_db));
        return false;
    }
    if (options->storage.wal != Wal::Off) {
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
    rc = sqlite3_exec(
        user_db,
        "CREATE TABLE IF NOT EXISTS users (uid INTEGER PRIMARY KEY, username TEXT UNIQUE, password TEXT, "
        "search_engine TEXT);",
        0, 0, 0
    );
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create users table: %s\n", sqlite3_errmsg(user_db));
        logger.write(Logger::Error, "userdb", std::string("无法创建用户表: ") + sqlite3_errmsg(user_db));
        return false;
    }
    rc = sqlite3_exec(
        user_db,
        "CREATE TABLE IF NOT EXISTS histories (id INTEGER PRIMARY KEY, uid INTEGER NOT NULL, answer TEXT "
        "NOT NULL, history TEXT NOT NULL, dictionary TEXT, grader TEXT NOT NULL, num_guess INTEGER NOT "
        "NULL, timestamp INTEGER NOT NULL, FOREIGN KEY(uid) REFERENCES users(uid) ON DELETE CASCADE);",
        0, 0, 0
    );
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

    if (options->storage.wal == Wal::Checkpoint) {
        logger.write(Logger::Debug, "userdb", "执行 WAL 检查点以合并数据");
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
        std::string hashed_password = (const char*) sqlite3_column_text(stmt, 1);
        if (bcrypt::validatePassword(password, hashed_password)) {
            uid = sqlite3_column_int(stmt, 0);
            search_engine = (const char*) sqlite3_column_text(stmt, 2);
            if (!options->dict.searchEngines.count(search_engine)) {
                search_engine = options->dict.searchEngine;
            }
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
    sqlite3_bind_text(stmt, 3, options->dict.searchEngine.c_str(), -1, SQLITE_STATIC);
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
        std::string hashed_password = (const char*) sqlite3_column_text(stmt, 0);
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
    if (user_db == nullptr || !options->dict.searchEngines.count(search_engine)) return false;

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

bool write_history(std::string answer, std::string history, int num_guess, std::string dict, std::string grader) {
    if (user_db == nullptr) return false;
    if (uid == -1) return true;

    long long current_time = std::time(nullptr);

    std::string sql = "INSERT INTO histories (uid, answer, history, num_guess, dictionary, grader, timestamp) VALUES (?, ?, ?, ?, ?, ?, ?)";
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
    sqlite3_bind_text(stmt, 5, dict.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, grader.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 7, current_time);
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
    search_engine = options->dict.searchEngine;
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
        const char* answer = (const char*) sqlite3_column_text(stmt, 1);
        long long timestamp = sqlite3_column_int64(stmt, 2);
        result.push_back(tr(msg::HistoryEntry {id, time_to_string(timestamp), answer}));
        history_ids.push_back(id);
        rc = sqlite3_step(stmt);
    }
    sqlite3_finalize(stmt);
    return result;
}

std::string get_history_detail(int id) {
    if (user_db == nullptr) return tr(Msg::HintInvalidHistory);

    std::string sql = "SELECT answer, history, num_guess, dictionary, grader, timestamp FROM histories WHERE id = ?";
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(user_db, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        logger.write(Logger::Error, "userdb", std::string("无法准备语句: ") + sqlite3_errmsg(user_db));
        return tr(Msg::HintInvalidHistory);
    }
    sqlite3_bind_int(stmt, 1, id);
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        if (rc == SQLITE_DONE) {
            sqlite3_finalize(stmt);
            return tr(Msg::HintInvalidHistory);
        }
        logger.write(Logger::Error, "userdb", std::string("无法获取历史记录: ") + sqlite3_errmsg(user_db));
        sqlite3_finalize(stmt);
        return tr(Msg::HintInvalidHistory);
    }
    int count = sqlite3_column_int(stmt, 2);
    auto result = count >= 0 ? tr(msg::SlotNumGuess {count}) : tr(Msg::HintFailedToGuess);
    const auto dict = column_text(stmt, 3);
    const auto grader = column_text(stmt, 4);
    auto content = tr(msg::HistoryDetail {
        id, time_to_string(sqlite3_column_int64(stmt, 5), true),
        tr(msg::FieldValue {tr(Msg::SetupDictionary), dict_display_name(dict)}),
        tr(msg::FieldValue {tr(Msg::SetupGrader), grader_display_name(grader)}),
        column_text(stmt, 0), result
    });
    content += column_text(stmt, 1);
    sqlite3_finalize(stmt);
    return content;
}
