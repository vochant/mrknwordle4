#include "char.hpp"

std::vector<std::string> Charset;

void InitCharset() {
	Charset.clear();
	Charset.push_back("│");
	Charset.push_back("─");
	Charset.push_back("┘");
	Charset.push_back("└");
	Charset.push_back("┐");
	Charset.push_back("┌");
	Charset.push_back("┤");
	Charset.push_back("├");
	Charset.push_back("┴");
	Charset.push_back("┬");
	Charset.push_back("┼");
	Charset.push_back("●");
	Charset.push_back("○");
	Charset.push_back("■");
	Charset.push_back("★");
}

std::string Lookup(Char id) {
	return Charset[(int)id];
}

// -*-*-Unused-*-*-
unsigned int getpoint(const char* const str, int& breaks) {
	unsigned int ch0 = str[0];
	if (ch0 < 0x80) {
		breaks = 0;
		return ch0;
	}
	unsigned int ch1 = str[1];
	if ((ch0 & 0xE0) == 0xC0) {
		breaks = 1;
		return ((ch0 & 0x1F) << 6) | (ch1 & 0x3F);
	}
	unsigned int ch2 = str[2];
	if ((ch0 & 0xF0) == 0xE0) {
		breaks = 2;
		return ((ch0 & 0x0F) << 12) | ((ch1 & 0x3F) << 6) | (ch2 & 0x3F);
	}
	unsigned int ch3 = str[3];
	if ((ch0 & 0xF8) == 0xF0) {
		breaks = 3;
		return ((ch0 & 0x07) << 18) | ((ch1 & 0x3F) << 12) | ((ch2 & 0x3F) << 6) | (ch3 & 0x3F);
	}
	breaks = 0;
	return 0;
}