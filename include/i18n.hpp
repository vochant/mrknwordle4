#pragma once

#include <string>
#include <filesystem>

void addTranslation(std::string lang, std::string key, std::string value);
void loadLanguage(std::string langname, std::filesystem::path langfile);

void bindLanguage(std::string lang);

std::string translate(std::string input);