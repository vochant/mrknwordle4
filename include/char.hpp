#pragma once

#include <string>

enum class Char { UD, LR, UL, UR, DL, DR, UDL, UDR, ULR, DLR, UDLR };
std::string Lookup(Char id);
