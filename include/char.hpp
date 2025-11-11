#pragma once

#include <vector>
#include <string>

enum class Char {
	UD, LR, UL, UR, DL, DR, UDL, UDR, ULR, DLR, UDLR,
	FILLED_CIRCLE, FRAME_CIRCLE, SMALL_BLOCK, STAR
};

extern std::vector<std::string> Charset;

void InitCharset();
std::string Lookup(Char id);