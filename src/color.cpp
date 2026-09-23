#include "color.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <limits>
#include <map>
#include <stdexcept>

namespace {
    const std::map<std::string, int> namedColors = {
#include "css_colors.inc"
    };

    const std::pair<char, uint16_t> styleNames[] = {
        {'b', Bold},  {'i', Italic},  {'u', Underline}, {'d', Dim},
        {'k', Blink}, {'r', Reverse}, {'s', Strike}
    };
} // namespace

Color parse_color(const std::string& expr) {
    if (expr.empty() || expr.size() > 256) {
        throw std::invalid_argument("Invalid color expression length");
    }
    Color res;
    size_t suf = expr.find(',');
    if (expr == "hidden") return Color(-1);
    if (suf != std::string::npos && expr.find(',', suf + 1) != std::string::npos) {
        throw std::invalid_argument("A color expression may contain only one comma");
    }
    {
        auto base = expr.substr(0, suf);
        if (base.compare(0, 5, "term:") == 0) {
            res.value = 0;
            size_t pos = 5;
            const std::string comps = "RGBL";
            const int bits[] = {4, 2, 1, 8};
            for (size_t i = 0; i < comps.size(); i++) {
                if (pos < base.size() && base[pos] == comps[i]) {
                    res.value |= bits[i];
                    pos++;
                }
            }
            if (pos != base.size()) {
                throw std::invalid_argument("Expected term:R?G?B?L?");
            }
        }
        else if (base.compare(0, 4, "rgb:") == 0) {
            auto digits = base.substr(4);
            if (digits.size() != 3 && digits.size() != 6 ||
                !std::all_of(digits.begin(), digits.end(), [](unsigned char letter) {
                    return letter >= '0' && letter <= '9' ||
                        letter >= 'a' && letter <= 'f' ||
                        letter >= 'A' && letter <= 'F';
                }
            )) {
                throw std::invalid_argument("Expected rgb:RGB or rgb:RRGGBB");
            }
            if (digits.size() == 3) {
                std::string ex;
                for (char digit : digits) {
                    ex += digit;
                    ex += digit;
                }
                digits = ex;
            }
            res.value = ColorRgbTag | std::stoi(digits, nullptr, 16);
        }
        else {
            std::transform(base.begin(), base.end(), base.begin(), [](unsigned char letter) {
                return std::tolower(letter);
            });
            auto found = namedColors.find(base);
            if (found == namedColors.end()) throw std::invalid_argument("Unknown CSS color: " + base);
            res.value = ColorRgbTag | found->second;
        }
    }
    if (suf != std::string::npos) {
        auto styles = expr.substr(suf + 1);
        for (char style : styles) {
            bool valid = false;
            for (auto [name, flag] : styleNames) {
                if (style == name) {
                    if (res.styles & flag) {
                        throw std::invalid_argument("Duplicate color style");
                    }
                    res.styles |= flag;
                    valid = true;
                    break;
                }
            }
            if (!valid) {
                throw std::invalid_argument("Unknown color style: " + std::string(1, style));
            }
        }
    }
    return res;
}

std::string colorexpr(Color color) {
    if (color.isHidden()) return "hidden";
    std::string res;
    if (color.value <= 15) {
        res = "term:";
        if (color.value & 4) res += 'R';
        if (color.value & 2) res += 'G';
        if (color.value & 1) res += 'B';
        if (color.value & 8) res += 'L';
    }
    else {
        char hex[11];
        std::snprintf(hex, sizeof(hex), "rgb:%06x", (unsigned) rgbValue(color.value));
        res = hex;
    }
    bool hasStyles = false;
    for (auto [name, flag] : styleNames) {
        if (!(color.styles & flag)) continue;
        if (!hasStyles) res += ',';
        res += name;
        hasStyles = true;
    }
    return res;
}

std::string colorescape(Color foreground, Color background) {
    return "\x02" + colorexpr(foreground) + ";" + colorexpr(background) + "\x03";
}

int nearest_term_color(int color) {
    if (isRgbColor(color)) color = rgbValue(color);
    if (color >= 0 && color <= 15) return color;
    const int palette[] = {
        0x000000, 0x000080, 0x008000, 0x008080, 0x800000, 0x800080, 0x808000, 0xc0c0c0,
        0x808080, 0x0000ff, 0x00ff00, 0x00ffff, 0xff0000, 0xff00ff, 0xffff00, 0xffffff
    };
    int best = 0, dist = std::numeric_limits<int>::max();
    for (int i = 0; i < 16; i++) {
        int cand = 0;
        for (int shift : {0, 8, 16}) {
            int diff = ((color >> shift) & 255) - ((palette[i] >> shift) & 255);
            cand += diff * diff;
        }
        if (cand < dist) {
            dist = cand;
            best = i;
        }
    }
    return best;
}
