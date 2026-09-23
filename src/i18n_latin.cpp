#include "i18n_latin.hpp"

#include <stdexcept>
#include <unicode/dcfmtsym.h>
#include <unicode/decimfmt.h>
#include <unicode/dtfmtsym.h>
#include <unicode/gregocal.h>
#include <unicode/plurrule.h>
#include <unicode/smpdtfmt.h>

namespace {
    icu::UnicodeString unicode(const char* value) { return icu::UnicodeString::fromUTF8(value); }

    void checked(UErrorCode status) {
        if (U_FAILURE(status)) {
            throw std::runtime_error(std::string("Cannot initialize Latin formatting: ") + u_errorName(status));
        }
    }
} // namespace

namespace latin {
    std::unique_ptr<icu::NumberFormat> number_format() {
        UErrorCode status = U_ZERO_ERROR;
        icu::DecimalFormatSymbols symbols(icu::Locale::getRoot(), status);
        symbols.setSymbol(icu::DecimalFormatSymbols::kDecimalSeparatorSymbol, unicode(","));
        symbols.setSymbol(icu::DecimalFormatSymbols::kGroupingSeparatorSymbol, unicode("\xC2\xA0"));
        auto result = std::make_unique<icu::DecimalFormat>(unicode("#,##0.###"), symbols, status);
        checked(status);
        return result;
    }

    std::unique_ptr<icu::DateFormat> date_format(bool detailed) {
        UErrorCode status = U_ZERO_ERROR;
        icu::DateFormatSymbols symbols(icu::Locale::getRoot(), status);
        const icu::UnicodeString months[] = {
            unicode("Ianuarii"), unicode("Februarii"), unicode("Martii"),
            unicode("Aprilis"), unicode("Maii"), unicode("Iunii"),
            unicode("Iulii"), unicode("Augusti"), unicode("Septembris"),
            unicode("Octobris"), unicode("Novembris"), unicode("Decembris")
        };
        const icu::UnicodeString shortMonths[] = {
            unicode("Ian"), unicode("Feb"), unicode("Mar"), unicode("Apr"),
            unicode("Mai"), unicode("Iun"), unicode("Iul"), unicode("Aug"),
            unicode("Sep"), unicode("Oct"), unicode("Nov"), unicode("Dec")
        };
        const icu::UnicodeString weekdays[] = {
            unicode(""), unicode("dies Solis"),
            unicode("dies Lunae"), unicode("dies Martis"),
            unicode("dies Mercurii"), unicode("dies Iovis"),
            unicode("dies Veneris"), unicode("dies Saturni")
        };
        const icu::UnicodeString shortWeekdays[] = {
            unicode(""), unicode("Sol"), unicode("Lun"), unicode("Mar"),
            unicode("Mer"), unicode("Iov"), unicode("Ven"), unicode("Sat")
        };
        const icu::UnicodeString eras[] = {unicode("a.C.n."), unicode("p.C.n.")};
        symbols.setMonths(months, 12);
        symbols.setShortMonths(shortMonths, 12);
        symbols.setWeekdays(weekdays, 8);
        symbols.setShortWeekdays(shortWeekdays, 8);
        symbols.setEras(eras, 2);
        auto pattern = detailed ? "EEEE, 'die' d MMMM y G 'de' HH:mm:ss" : "d M y G, HH:mm:ss";
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
        if (!result) throw std::runtime_error("Cannot initialize Latin plural rules");
        return result;
    }
} // namespace latin
