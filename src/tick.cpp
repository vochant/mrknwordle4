#include "tick.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

constexpr int TICK_SLEEP = 25;

void tick() {
	Sleep(TICK_SLEEP);
}