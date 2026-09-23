#pragma once

#include <sqlite3.h>
#include "options.hpp"
#include "game_context.hpp"
#include <ctime>

extern sqlite3* user_db;
extern int uid;
extern std::string username, search_engine;
extern std::vector<int> history_ids;

bool init_user_db();
void close_user_db();
int login_user(std::string username, std::string password);
int create_user(std::string username, std::string password);
bool check_password(std::string password);
bool change_password(std::string password);
bool change_username(std::string username);
bool change_search_engine(std::string search_engine);
bool write_history(std::string answer, std::string history, int num_guess, std::string dictionary, std::string grader);
int count_history();
std::vector<std::string> get_history(int offset, int limit);
std::string get_history_detail(int id);

void logout_user();
bool remove_user();
