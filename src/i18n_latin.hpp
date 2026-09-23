#pragma once

#include <memory>
#include <string>
#include <unicode/utypes.h>

U_NAMESPACE_BEGIN
class DateFormat;
class NumberFormat;
class PluralRules;
U_NAMESPACE_END

namespace latin {
    std::unique_ptr<icu::NumberFormat> number_format();
    std::unique_ptr<icu::DateFormat> date_format(bool detailed);
    std::unique_ptr<icu::PluralRules> plural_rules(bool ordinal);
} // namespace latin
