#include "cursor.hpp"
#include "global.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdio>
#include "options.hpp"

class Win32Cursor : public Cursor {
public:
	void show() override {
		CONSOLE_CURSOR_INFO cursorInfo;
		GetConsoleCursorInfo(hOutput, &cursorInfo);
		cursorInfo.bVisible = TRUE;
   		SetConsoleCursorInfo(hOutput, &cursorInfo);
	}

	void hide() override {
		CONSOLE_CURSOR_INFO cursorInfo;
		GetConsoleCursorInfo(hOutput, &cursorInfo);
		cursorInfo.bVisible = FALSE;
   		SetConsoleCursorInfo(hOutput, &cursorInfo);
	}
};

class VT100Cursor : public Cursor {
public:
    void show() override {
        fputs("\x1b[?25h", stdout);
    }

    void hide() override {
        fputs("\x1b[?25l", stdout);
    }
};

Cursor* cursorController = nullptr;

void init_cursor() {
    if (options->virtualTerminal) cursorController = new VT100Cursor();
    else cursorController = new Win32Cursor();
}