#include "i18n_mf.hpp"

#include <stdexcept>
#include <unicode/dtfmtsym.h>
#include <unicode/gregocal.h>
#include <unicode/plurrule.h>
#include <unicode/smpdtfmt.h>

namespace {
    icu::UnicodeString unicode(const char* value) { return icu::UnicodeString::fromUTF8(value); }

    void checked(UErrorCode status) {
        if (U_FAILURE(status)) {
            throw std::runtime_error(std::string("Cannot initialize qmf date formatting: ") + u_errorName(status));
        }
    }
} // namespace

namespace minpoe {
    std::unique_ptr<icu::DateFormat> date_format(bool detailed) {
        UErrorCode status = U_ZERO_ERROR;
        icu::DateFormatSymbols symbols(icu::Locale::getRoot(), status);
        const icu::UnicodeString months[] = {
            unicode("Plotilos"), unicode("Certōlos"), unicode("Hacralos"),
            unicode("Hiraelos"), unicode("Acealos"), unicode("Ennelos"),
            unicode("Iduelos"), unicode("Suvalos"), unicode("Vollos"),
            unicode("Carulos"), unicode("Prebulos"), unicode("Ixeolos")
        };
        const icu::UnicodeString shortMonths[] = {
            unicode("Plo"), unicode("Cer"), unicode("Hac"), unicode("Hir"),
            unicode("Ace"), unicode("Enn"), unicode("Idu"), unicode("Suv"),
            unicode("Vol"), unicode("Car"), unicode("Pre"), unicode("Ixe")
        };
        const icu::UnicodeString weekdays[] = {
            unicode(""), unicode("Igumstē"), unicode("Malstē"),
            unicode("Beostē"), unicode("Novumstē"), unicode("Muostē"),
            unicode("Tiseostē"), unicode("Andumstē")
        };
        const icu::UnicodeString shortWeekdays[] = {
            unicode(""), unicode("Igu"), unicode("Mal"), unicode("Beo"),
            unicode("Nov"), unicode("Muo"), unicode("Tis"), unicode("And")
        };
        symbols.setMonths(months, 12);
        symbols.setShortMonths(shortMonths, 12);
        symbols.setWeekdays(weekdays, 8);
        symbols.setShortWeekdays(shortWeekdays, 8);
        const auto pattern = detailed ? "EEEE d MMMM, y 'ne' H:mm:ss" : "d/M/yy, H:mm:ss";
        auto result = std::make_unique<icu::SimpleDateFormat>(unicode(pattern), symbols, status);
        icu::GregorianCalendar calendar(icu::Locale::getRoot(), status);
        result->setCalendar(calendar);
        result->setDateFormatSymbols(symbols);
        checked(status);
        return result;
    }

    std::unique_ptr<icu::PluralRules> plural_rules(bool ordinal) {
        UErrorCode status = U_ZERO_ERROR;
        auto result = std::unique_ptr<icu::PluralRules>(
            icu::PluralRules::createRules(unicode(ordinal ? "other:" : "one: n = 1; other:"), status)
        );
        checked(status);
        if (!result) throw std::runtime_error("Cannot initialize qmf plural rules");
        return result;
    }
} // namespace mf
