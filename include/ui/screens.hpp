#pragma once

#include <memory>

class Widget;
class Layout;

std::unique_ptr<Widget> MainMenu();
std::unique_ptr<Widget> GameMenu();
std::unique_ptr<Widget> GameAdvancedMenu();
void resetGameSetup();
std::unique_ptr<Widget> GameMain();
std::unique_ptr<Widget> DictSelector();
std::unique_ptr<Widget> DictAdvancedSelector();
void resetDictionarySetup();
std::unique_ptr<Layout> DictMenu();
std::unique_ptr<Layout> DictMain();
std::unique_ptr<Layout> DictFilter();
std::unique_ptr<Layout> WordViewer();
std::unique_ptr<Layout> SearchEngineSelect();
std::unique_ptr<Layout> UserMenuNotLogged();
std::unique_ptr<Widget> UserMenuLogged();
std::unique_ptr<Widget> UserLogin();
std::unique_ptr<Widget> UserRegister();
std::unique_ptr<Widget> UserSearch();
std::unique_ptr<Widget> UserSecurity();
std::unique_ptr<Widget> UserDelete();
std::unique_ptr<Widget> HistoryMenu();
std::unique_ptr<Widget> HistoryEntry();
std::unique_ptr<Widget> PluginMenu();
std::unique_ptr<Widget> PluginEntry();
