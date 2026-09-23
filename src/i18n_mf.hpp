#pragma once

#include <memory>
#include <unicode/utypes.h>

U_NAMESPACE_BEGIN
class DateFormat;
class PluralRules;
U_NAMESPACE_END

namespace minpoe {
    std::unique_ptr<icu::DateFormat> date_format(bool detailed);
    std::unique_ptr<icu::PluralRules> plural_rules(bool ordinal);
} // namespace mf
