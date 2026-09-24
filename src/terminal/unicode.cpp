#include "terminal/terminal.hpp"
#include <unicode/brkiter.h>
#include <unicode/normalizer2.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>
#include <unicode/utf8.h>
#include <stdexcept>

std::vector<std::string> split_graphemes(const std::string& text) {
    UErrorCode status = U_ZERO_ERROR;
    thread_local std::unique_ptr<icu::BreakIterator> it(
        icu::BreakIterator::createCharacterInstance(icu::Locale::getRoot(), status)
    );
    if (U_FAILURE(status) || !it) throw std::runtime_error("Cannot initialize grapheme iterator");
    auto unicode = icu::UnicodeString::fromUTF8(text);
    it->setText(unicode);
    std::vector<std::string> clusters;
    int32_t st = it->first();
    for (int32_t ex = it->next(); ex != icu::BreakIterator::DONE; st = ex, ex = it->next()) {
        std::string cluster;
        unicode.tempSubStringBetween(st, ex).toUTF8String(cluster);
        clusters.push_back(std::move(cluster));
    }
    return clusters;
}

std::string encode_utf8(char32_t cp) {
    if (cp > 0x10ffff || cp >= 0xd800 && cp <= 0xdfff) return {};
    icu::UnicodeString unicode((UChar32) cp);
    std::string result;
    unicode.toUTF8String(result);
    return result;
}

TerminalGlyph prepare_glyph(const std::string& cluster, int amb, bool singleBmp) {
    const TerminalGlyph repl {"?", 1, true};
    if (cluster.empty() || cluster.size() > 128) return repl;
    UErrorCode status = U_ZERO_ERROR;
    const auto* normalizer = icu::Normalizer2::getNFCInstance(status);
    if (U_FAILURE(status)) throw std::runtime_error("Cannot initialize NFC normalizer");
    icu::UnicodeString normalized;
    normalizer->normalize(icu::UnicodeString::fromUTF8(cluster), normalized, status);
    if (U_FAILURE(status)) return repl;
    int cnt = 0, width = 0;
    for (int32_t offset = 0; offset < normalized.length();) {
        UChar32 cp = normalized.char32At(offset);
        offset += U16_LENGTH(cp);
        cnt++;
        const auto category = u_charType(cp);
        if (cp == 0xfffd ||
            category == U_UNASSIGNED ||
            category == U_PRIVATE_USE_CHAR ||
            category == U_CONTROL_CHAR ||
            category == U_FORMAT_CHAR ||
            category == U_SURROGATE ||
            category == U_LINE_SEPARATOR ||
            category == U_PARAGRAPH_SEPARATOR ||
            u_hasBinaryProperty(cp, UCHAR_DEFAULT_IGNORABLE_CODE_POINT) ||
            u_hasBinaryProperty(cp, UCHAR_EMOJI_PRESENTATION) ||
            u_hasBinaryProperty(cp, UCHAR_EMOJI_MODIFIER) ||
            cp == 0x20e3 ||
            singleBmp && cp > 0xffff
        ) return repl;
        if (category == U_NON_SPACING_MARK || category == U_ENCLOSING_MARK) {
            if (!width || singleBmp || cnt > 4) return repl;
            continue;
        }
        if (width || (singleBmp && cnt > 1) || category == U_COMBINING_SPACING_MARK) return repl;
        auto eaw = u_getIntPropertyValue(cp, UCHAR_EAST_ASIAN_WIDTH);
        if (eaw == U_EA_AMBIGUOUS && amb != 1 && amb != 2) return repl;
        width = eaw == U_EA_FULLWIDTH || eaw == U_EA_WIDE ? 2 :
            eaw == U_EA_AMBIGUOUS ? amb : 1;
    }
    if (!width) return repl;
    std::string res;
    normalized.toUTF8String(res);
    return {res, width, false};
}

TerminalGlyph Terminal::prepareGlyph(const std::string& cluster) const {
    auto glyph = ::prepare_glyph(cluster, 1, singleBmpCells());
    if (glyph.replaced) return glyph;

    auto found = glyphWidths.find(glyph.text);
    const int width = found == glyphWidths.end() ?
        glyphWidths.emplace(glyph.text, measureGlyphWidth(glyph.text)).first->second :
        found->second;
    if (width != 1 && width != 2) return {"?", 1, true};
    glyph.width = width;
    return glyph;
}
