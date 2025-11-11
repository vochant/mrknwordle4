#pragma once

class Cursor {
public:
	virtual void show() = 0;
	virtual void hide() = 0;
};

void init_cursor();

extern Cursor* cursorController;