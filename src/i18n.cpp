#include "i18n.hpp"
#include "i18n_latin.hpp"
#include "i18n_mf.hpp"
#include "locale.hpp"
#include "options.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <unicode/datefmt.h>
#include <unicode/msgfmt.h>
#include <unicode/messagepattern.h>
#include <unicode/numfmt.h>
#include <unicode/plurfmt.h>
#include <unicode/plurrule.h>
#include <unicode/timezone.h>
#include <unicode/utf8.h>

using nlohmann::json;

namespace {
    std::atomic<const I18n*> service {nullptr};

    icu::UnicodeString unicode(const std::string& value) { return icu::UnicodeString::fromUTF8(value); }

    std::string utf8(const icu::UnicodeString& value) {
        std::string result;
        value.toUTF8String(result);
        return result;
    }

    void checked(UErrorCode status, const std::string& context) {
        if (U_FAILURE(status)) {
            throw std::invalid_argument(context + ": " + u_errorName(status));
        }
    }

    json read_json(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("Cannot open language resource: " + path.string());
        }
        return json::parse(input);
    }

    std::unique_ptr<icu::TimeZone> make_time_zone(const std::string& name) {
        if (name == "local") {
            return std::unique_ptr<icu::TimeZone>(icu::TimeZone::createDefault());
        }
        UErrorCode status = U_ZERO_ERROR;
        icu::UnicodeString canonical;
        icu::TimeZone::getCanonicalID(unicode(name), canonical, status);
        checked(status, "Invalid time zone");
        if (canonical == icu::UnicodeString("Etc/Unknown")) {
            throw std::invalid_argument("Invalid time zone: " + name);
        }
        return std::unique_ptr<icu::TimeZone>(icu::TimeZone::createTimeZone(canonical));
    }

    icu::Locale make_locale(const std::string& language, const std::string& calendar) {
        UErrorCode status = U_ZERO_ERROR;
        auto locale = icu::Locale::forLanguageTag(language, status);
        checked(status, "Invalid formatting locale");
        if (calendar != "default") locale.setKeywordValue("calendar", calendar.c_str(), status);
        checked(status, "Invalid calendar");
        return locale;
    }

    struct Profile {
        bool isLatin = false;
        bool isMinpoe = false;
        icu::Locale locale;
        std::unique_ptr<icu::PluralRules> cardinal;
        std::unique_ptr<icu::PluralRules> ordinal;
        std::unique_ptr<icu::NumberFormat> numbers;
        std::unique_ptr<icu::DateFormat> compact;
        std::unique_ptr<icu::DateFormat> detailed;
    };

    Profile make_profile(const std::string& language, const LocaleOptions& settings) {
        Profile result;
        result.isLatin = language == "la";
        result.isMinpoe = language == "qmf";
        if ((result.isLatin || result.isMinpoe) &&
            settings.calendar != "default" &&
            settings.calendar != "gregorian"
        ) {
            throw std::invalid_argument("Custom date formatting supports only the Gregorian calendar");
        }
        result.locale = make_locale(language, result.isLatin ? "default" : settings.calendar);
        UErrorCode status = U_ZERO_ERROR;
        if (result.isLatin) {
            result.cardinal = latin::plural_rules(false);
            result.ordinal = latin::plural_rules(true);
            result.numbers = latin::number_format();
            result.compact = latin::date_format(false);
            result.detailed = latin::date_format(true);
        }
        else {
            result.cardinal.reset(icu::PluralRules::forLocale(result.locale, UPLURAL_TYPE_CARDINAL, status));
            result.ordinal.reset(icu::PluralRules::forLocale(result.locale, UPLURAL_TYPE_ORDINAL, status));
            result.numbers.reset(icu::NumberFormat::createInstance(result.locale, status));
            if (result.isMinpoe) {
                result.cardinal = minpoe::plural_rules(false);
                result.ordinal = minpoe::plural_rules(true);
                result.compact = minpoe::date_format(false);
                result.detailed = minpoe::date_format(true);
            }
            else {
                result.compact.reset(icu::DateFormat::createDateTimeInstance(
                    icu::DateFormat::SHORT, icu::DateFormat::MEDIUM, result.locale
                ));
                result.detailed.reset(icu::DateFormat::createDateTimeInstance(
                    icu::DateFormat::FULL, icu::DateFormat::MEDIUM, result.locale
                ));
            }
        }
        checked(status, "Cannot construct formatting profile");
        if (!result.cardinal || !result.ordinal || !result.numbers || !result.compact || !result.detailed) {
            throw std::invalid_argument("Incomplete formatting profile");
        }
        if (!result.isLatin &&
            settings.calendar != "default" &&
            settings.calendar != result.compact->getCalendar()->getType()
        ) {
            throw std::invalid_argument("Unsupported calendar: " + settings.calendar);
        }
        auto zone = make_time_zone(settings.timeZone);
        result.compact->setTimeZone(*zone);
        result.detailed->setTimeZone(*zone);
        return result;
    }

    void inject_plural_rules(
        icu::MessageFormat& message, const std::string& pattern,
        const Profile& profile, const std::string& context
    ) {
        if (!profile.isLatin && !profile.isMinpoe) return;
        UErrorCode status = U_ZERO_ERROR;
        UParseError error {};
        icu::MessagePattern parsed(unicode(pattern), &error, status);
        checked(status, context);
        const auto source = unicode(pattern);
        for (int32_t index = 0; index < parsed.countParts(); index++) {
            if (parsed.getPartType(index) != UMSGPAT_PART_TYPE_ARG_START) continue;
            const auto type = parsed.getPart(index).getArgType();
            if (type != UMSGPAT_ARG_TYPE_PLURAL && type != UMSGPAT_ARG_TYPE_SELECTORDINAL) continue;

            const auto name = parsed.getSubstring(parsed.getPart(index + 1));
            auto styleStart = parsed.getPart(index + 1).getLimit();
            for (int commas = 0; commas < 2; commas++) {
                styleStart = source.indexOf(',', styleStart);
                if (styleStart < 0) throw std::invalid_argument("Malformed plural argument: " + context);
                styleStart++;
            }
            while (styleStart < source.length()) {
                const auto character = source.charAt(styleStart);
                if (character != ' ' && character != '\t' && character != '\r' && character != '\n') break;
                styleStart++;
            }
            const auto styleEnd = parsed.getPart(parsed.getLimitPartIndex(index)).getIndex();
            const auto& rules = type == UMSGPAT_ARG_TYPE_SELECTORDINAL ? *profile.ordinal : *profile.cardinal;
            icu::PluralFormat plural(profile.locale, rules, source.tempSubStringBetween(styleStart, styleEnd), status);
            plural.setNumberFormat(profile.numbers.get(), status);
            message.setFormat(name, plural, status);
            checked(status, context);
        }
    }

} // namespace

std::string safe_text(const std::string& text) {
    std::string result;
    int32_t offset = 0;
    while (offset < text.size()) {
        int32_t start = offset;
        UChar32 character;
        U8_NEXT(text.data(), offset, text.size(), character);
        if (character < 0) throw std::invalid_argument("Invalid UTF-8 text");
        if ((character < 32 && character != '\n') ||
            (character >= 127 && character <= 159) ||
            (character >= 0x202a && character <= 0x202e) ||
            (character >= 0x2066 && character <= 0x2069)
        ) result += '?';
        else result.append(text, start, offset - start);
    }
    return result;
}

struct I18n::Impl {
    struct Message {
        std::unique_ptr<icu::MessageFormat> format;
    };
    mutable std::mutex mutex;
    Profile profile;
    std::map<std::string, Message> messages;
};

I18n::I18n(const std::filesystem::path& directory, const LocaleOptions& settings) : impl(std::make_unique<Impl>()) {
    const auto index = read_json(directory / "locales.json");
    const auto base = index.at("base").get<std::string>();
    const auto localeList = index.at("locales").get<std::vector<std::string>>();
    const auto aliases = index.value("aliases", std::map<std::string, std::string> {});

    auto requested = normalize_lang(settings.language);
    std::string selected;
    while (!requested.empty()) {
        auto alias = aliases.find(requested);
        auto candidate = alias == aliases.end() ? requested : alias->second;
        if (std::find(localeList.begin(), localeList.end(), candidate) != localeList.end()) {
            selected = candidate;
            break;
        }
        auto separator = requested.rfind('-');
        if (separator == std::string::npos) break;
        requested.resize(separator);
    }
    if (selected.empty()) selected = base;

    const auto document = read_json(directory / (selected + ".json"));
    const auto& messages = document.at("messages");
    impl->profile = make_profile(selected, settings);
    for (const auto& message : messages.items()) {
        const auto pattern = message.value().get<std::string>();
        UErrorCode status = U_ZERO_ERROR;
        UParseError error {};
        auto format = std::make_unique<icu::MessageFormat>(
            unicode(pattern), impl->profile.locale, error, status
        );
        checked(status, selected + ":" + message.key());
        inject_plural_rules(*format, pattern, impl->profile, selected + ":" + message.key());
        impl->messages.emplace(message.key(), Impl::Message {std::move(format)});
    }
}

I18n::~I18n() = default;

const I18n& I18n::active() {
    auto current = service.load();
    if (!current) throw std::logic_error("I18n is not initialized");
    return *current;
}

I18n::Binding::Binding(const I18n& current) {
    const I18n* expected = nullptr;
    if (!service.compare_exchange_strong(expected, &current)) throw std::logic_error("I18n already bound");
}

I18n::Binding::~Binding() { service.store(nullptr); }

std::string I18n::text(Msg message) const {
    auto index = (size_t) message;
    if (index >= std::size(msgIds)) throw std::out_of_range("Invalid message id");
    return render(msgIds[index], {});
}

std::string I18n::render(const std::string& id, std::initializer_list<Argument> arguments) const {
    std::lock_guard<std::mutex> guard(impl->mutex);
    std::vector<icu::UnicodeString> names;
    std::vector<icu::Formattable> values;
    for (const auto& argument : arguments) {
        names.push_back(unicode(argument.name));
        if (const auto* text = std::get_if<std::string>(&argument.value)) {
            values.emplace_back(unicode(safe_text(*text)));
        }
        else {
            values.emplace_back(std::get<int64_t>(argument.value));
        }
    }
    icu::UnicodeString result;
    UErrorCode status = U_ZERO_ERROR;
    impl->messages.at(id).format->format(names.data(), values.data(), values.size(), result, status);
    checked(status, "Cannot format " + id);
    return safe_text(utf8(result));
}

std::string I18n::number(int64_t value) const {
    std::lock_guard<std::mutex> guard(impl->mutex);
    icu::UnicodeString result;
    impl->profile.numbers->format(value, result);
    return safe_text(utf8(result));
}

std::string I18n::quantity(double value, bool ordinal) const {
    if (!std::isfinite(value) || value < 0 || value > 9007199254740991.0) {
        throw std::invalid_argument("Quantity must be finite and exactly representable");
    }
    std::lock_guard<std::mutex> guard(impl->mutex);
    const auto& profile = impl->profile;
    return utf8((ordinal ? profile.ordinal : profile.cardinal)->select(value));
}

std::string I18n::dateTime(int64_t seconds, bool detailed) const {
    if (seconds < -8640000000000LL || seconds > 8640000000000LL) throw std::out_of_range("Date exceeds ICU range");
    std::lock_guard<std::mutex> guard(impl->mutex);
    const auto& profile = impl->profile;
    icu::UnicodeString result;
    (detailed ? profile.detailed : profile.compact)->format(((UDate) seconds) * 1000, result);
    return safe_text(utf8(result));
}
