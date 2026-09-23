#pragma once

#include <cstdint>
#include <string>

enum ColorStyle : uint16_t { Bold = 1, Italic = 2, Underline = 4, Dim = 8, Blink = 16, Reverse = 32, Strike = 64 };

struct Color {
    int value = 7;
    uint16_t styles = 0;
    Color(int value = 7, uint16_t styles = 0) : value(value), styles(styles) {}
    bool isHidden() const { return value == -1; }
    bool operator==(const Color& other) const { return value == other.value && styles == other.styles; }
};

constexpr int ColorRgbTag = 0x10000000;
inline bool isRgbColor(int value) { return value >= 0 && (value & 0x10000000) == ColorRgbTag; }
inline int rgbValue(int value) { return value & 0xffffff; }

Color parse_color(const std::string& expr);
std::string colorexpr(Color color);
std::string colorescape(Color foreground, Color background = Color(0));
int nearest_term_color(int color);
