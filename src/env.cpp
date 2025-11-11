#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdlib>
#include "env.hpp"
#include <conio.h>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#ifndef ENABLE_VIRTUAL_TERMINAL_INPUT
#define ENABLE_VIRTUAL_TERMINAL_INPUT 0x0200
#endif

#include "global.h"
#include "logger.hpp"
#include "cursor.hpp"
#include "paintbrush/paintbrush.hpp"
#include "options.hpp"

#if defined(__DEBUG__) || defined(__DEBUG) || defined(DEBUG) || defined(_DEBUG)
Logger logger(Logger::Debug);
#else
Logger logger(Logger::Info);
#endif

void *hInput, *hOutput;

void cls(HANDLE hConsole) {
    COORD coordScreen = {0, 0};
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD dwConSize;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;
    if (!FillConsoleOutputCharacter(hConsole, ' ', dwConSize, coordScreen, &cCharsWritten)) return;
    if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) return;
    if (!FillConsoleOutputAttribute(hConsole, csbi.wAttributes, dwConSize, coordScreen, &cCharsWritten)) return;
    SetConsoleCursorPosition(hConsole, coordScreen);
}

Environment::Environment() {
    AUTOLOG(Logger::Debug);

    err = false;

    // init: 备份原有代码页
    if (options->codepage != -1) {
        logger.write(Logger::Debug, "INIT", "备份原有代码页");
        inputCP = GetConsoleCP();
        outputCP = GetConsoleOutputCP();

        // init: 切换到新代码页
        // 详见 <https://learn.microsoft.com/zh-cn/windows/win32/intl/code-page-identifiers>
        logger.write(Logger::Debug, "INIT", "切换到新代码页");
        SetConsoleCP(options->codepage);
        SetConsoleOutputCP(options->codepage);
    }

    // init: 获取标准输入输出句柄
    logger.write(Logger::Debug, "INIT", "获取标准输入输出句柄");
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    hInput = GetStdHandle(STD_INPUT_HANDLE);
    if (hOutput == INVALID_HANDLE_VALUE || hInput == INVALID_HANDLE_VALUE) {
        err = true;
        return;
    }

    // init: 启用鼠标事件响应，禁用文本选中
    logger.write(Logger::Debug, "INIT", "启用鼠标事件响应，禁用文本选中");
    GetConsoleMode(hInput, &mode);
    SetConsoleMode(hInput, mode & (~ENABLE_MOUSE_INPUT) & (~ENABLE_QUICK_EDIT_MODE) & (~ENABLE_INSERT_MODE) & (~ENABLE_VIRTUAL_TERMINAL_INPUT) | (ENABLE_EXTENDED_FLAGS));

    // init: 启用虚拟终端处理
    GetConsoleMode(hOutput, &outputMode); // 保存原始输出模式
    if (options->virtualTerminal) {
        logger.write(Logger::Debug, "INIT", "启用虚拟终端处理");
        if (!SetConsoleMode(hOutput, outputMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)) {
            logger.write(Logger::Warn, "INIT", "无法启用虚拟终端处理，回退到传统模式");
            options->virtualTerminal = false;
        } else {
            logger.write(Logger::Info, "INIT", "成功启用虚拟终端处理");
        }
    }

    // init: 隐藏光标
    init_cursor();
    logger.write(Logger::Debug, "INIT", "隐藏光标");
    cursorController->hide();

    // init: 重置控制台窗口
    logger.write(Logger::Debug, "INIT", "重置控制台窗口");
    cls(hOutput);
    COORD coord = {0, 0};
    SetConsoleCursorPosition(hOutput, coord);
    if (options->virtualTerminal) {
        fputs("\x1b[1m", stdout);
        fflush(stdout); // 确保虚拟终端序列被立即发送
    } else {
        SetConsoleTextAttribute(hOutput, 15);
    }
    SetConsoleTitleA("Mirekintoic Wordle 4.0-rc1");
}

Environment::~Environment() {
    AUTOLOG(Logger::Debug);

    if (options->codepage != -1) {
        // exit: 恢复原有代码页
        logger.write(Logger::Debug, "EXIT", "恢复原有代码页");
        SetConsoleCP(inputCP);
        SetConsoleOutputCP(outputCP);
    }
    if (err) return;

    // exit: 显示光标
    logger.write(Logger::Debug, "EXIT", "显示光标");
    cursorController->show();

    // exit: 重置控制台窗口
    logger.write(Logger::Debug, "EXIT", "重置控制台窗口");
    cls(hOutput);
    COORD coord = {0, 0};
    SetConsoleCursorPosition(hOutput, coord);
    if (options->virtualTerminal) {
        fputs("\x1b[0m", stdout);
        fflush(stdout); // 确保虚拟终端序列被立即发送
    } else {
        SetConsoleTextAttribute(hOutput, 7);
    }

    // exit: 恢复控制台模式
    logger.write(Logger::Debug, "EXIT", "恢复原有控制台模式");
    SetConsoleMode(hInput, mode);
    SetConsoleMode(hOutput, outputMode);
}

bool Environment::check()  {
    if (err) return true;

    CONSOLE_SCREEN_BUFFER_INFO csbi;

    if (!GetConsoleScreenBufferInfo(hOutput, &csbi)) {
        logger.write(Logger::Error, "CHECK", "无法获取控制台屏幕缓冲区信息");
        return true;
    }

    int columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    if (columns < 80 || rows < 25) {
        logger.write(Logger::Error, "CHECK", "控制台窗口过小");
        return true;
    }

    if (options->mouseControlling) {
        COORD fontSize = GetConsoleFontSize(hOutput, 0);
        int font_w = fontSize.X, font_h = fontSize.Y;
        if (font_w == 0 || font_h == 0) {
            PaintBrush pb;
            pb.rect(20, 9, 59, 16, false);
            pb.locate(28, 11);
            pb.text("致命错误 无法获取字体大小");
            pb.locate(25, 12);
            pb.text("您可能正在使用 Windows Terminal");
            pb.locate(22, 13);
            pb.text("请使用 conhost 运行或禁用鼠标控制模式");
            pb.locate(32, 14);
            pb.text("按任意键退出程序");
            getch();
            return true;
        }
    }

    logger.write(Logger::Info, "CHECK", "已完成初始化和检查");
    return false;
}