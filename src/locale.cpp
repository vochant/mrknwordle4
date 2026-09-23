#include "locale.hpp"
#include <unicode/locid.h>
#include <unicode/uloc.h>
#include <clocale>
#include <stdexcept>

std::string normalize_lang(const std::string& language) {
    if (language.empty() || language.size() > 128) throw std::invalid_argument("Invalid language tag length");
    UErrorCode status = U_ZERO_ERROR;
    char localeId[ULOC_FULLNAME_CAPACITY];
    int32_t parsed = 0;
    uloc_forLanguageTag(language.c_str(), localeId, sizeof(localeId), &parsed, &status);
    if (U_FAILURE(status) || parsed != language.size()) {
        throw std::invalid_argument("locale.language must be a valid BCP 47 tag: " + language);
    }
    auto locale = icu::Locale::forLanguageTag(language, status);
    auto canonical = locale.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || locale.isBogus()) throw std::invalid_argument("Invalid language tag: " + language);
    return canonical;
}

void init_text_locale() {
#ifdef _WIN32
    std::setlocale(LC_CTYPE, ".UTF-8");
#else
    std::setlocale(LC_CTYPE, "C.UTF-8");
#endif
}
