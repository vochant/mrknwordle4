#include "char.hpp"

std::string Lookup(Char id) {
    static const char* symbols[] = {"│", "─", "┘", "└", "┐", "┌", "┤", "├", "┴", "┬", "┼"};
    return symbols[(size_t) id];
}
