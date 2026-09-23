#pragma once

#include "i18n_messages.hpp"
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <string>
#include <variant>
#include <vector>
#include <nlohmann/json_fwd.hpp>

struct LocaleOptions;

class I18n {
public:
    struct Argument {
        std::string name;
        std::variant<std::string, int64_t> value;
    };
    I18n(const std::filesystem::path& directory, const LocaleOptions& settings);
    ~I18n();
    I18n(const I18n&) = delete;
    I18n& operator=(const I18n&) = delete;
    std::string text(Msg message) const;
    std::string render(const std::string& id, std::initializer_list<Argument> arguments) const;
    std::string number(int64_t value) const;
    std::string dateTime(int64_t seconds, bool detailed = false) const;
    std::string quantity(double value, bool ordinal = false) const;
    static const I18n& active();
    class Binding {
    public:
        explicit Binding(const I18n& service);
        ~Binding();
        Binding(const Binding&) = delete;
        Binding& operator=(const Binding&) = delete;
    };

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

std::string safe_text(const std::string& text);
inline std::string tr(Msg message) { return I18n::active().text(message); }

inline std::string tr(const msg::TemplateWelcome& v) {
    return I18n::active().render("template.welcome", {{"name", v.name}});
}

inline std::string tr(const msg::SlotNumGuess& v) {
    return I18n::active().render("slot.num_guess", {{"count", v.count}});
}

inline std::string tr(const msg::FilterRemoveWord& v) {
    return I18n::active().render("filter.removeWord", {{"word", v.word}});
}

inline std::string tr(const msg::ErrorGrader& v) {
    return I18n::active().render("error.grader", {{"detail", v.detail}});
}

inline std::string tr(const msg::ActionConfirm& v) {
    return I18n::active().render("action.confirm", {{"key", v.key}});
}

inline std::string tr(const msg::ActionCancel& v) {
    return I18n::active().render("action.cancel", {{"key", v.key}});
}

inline std::string tr(const msg::ActionYes& v) {
    return I18n::active().render("action.yes", {{"key", v.key}});
}

inline std::string tr(const msg::ActionNo& v) {
    return I18n::active().render("action.no", {{"key", v.key}});
}

inline std::string tr(const msg::Pagination& v) {
    return I18n::active().render("pagination", {{"page", v.page}, {"pages", v.pages}});
}

inline std::string tr(const msg::FieldValue& v) {
    return I18n::active().render("field.value", {{"label", v.label}, {"value", v.value}});
}

inline std::string tr(const msg::PluginSummary& v) {
    return I18n::active().render("plugin.summary", {
        {"name", v.name},
        {"id", v.id},
        {"version", v.version},
        {"status", v.status},
        {"author", v.author},
        {"license", v.license},
        {"url", v.url},
        {"description", v.description},
        {"diagnostics", v.diagnostics}
    });
}

inline std::string tr(const msg::HistoryDetail& v) {
    return I18n::active().render("history.detail", {
        {"id", v.id},
        {"date", v.date},
        {"dictionary", v.dictionary},
        {"grader", v.grader},
        {"answer", v.answer},
        {"result", v.result}
    });
}

inline std::string tr(const msg::HistoryEntry& v) {
    return I18n::active().render("history.entry", {{"id", v.id}, {"date", v.date}, {"answer", v.answer}});
}

#define WORDLE_KEY_MSG(Type, Id) inline std::string tr(const msg::Type& v) { return I18n::active().render(Id, {{"key", v.key}}); }

WORDLE_KEY_MSG(HintBack, "hint.back.shortcut")
WORDLE_KEY_MSG(EntryLogin, "entry.login.shortcut")
WORDLE_KEY_MSG(EntryRegister, "entry.register.shortcut")
WORDLE_KEY_MSG(EntrySearchSettings, "entry.search_settings.shortcut")
WORDLE_KEY_MSG(EntrySecuritySettings, "entry.security_settings.shortcut")
WORDLE_KEY_MSG(EntryLogout, "entry.logout.shortcut")
WORDLE_KEY_MSG(EntryDeleteAccount, "entry.delete_account.shortcut")
WORDLE_KEY_MSG(EntryGame, "entry.game.shortcut")
WORDLE_KEY_MSG(EntryHistory, "entry.history.shortcut")
WORDLE_KEY_MSG(EntryUser, "entry.user.shortcut")
WORDLE_KEY_MSG(EntryPlugins, "entry.plugins.shortcut")
WORDLE_KEY_MSG(EntryDictionary, "entry.dictionary.shortcut")
WORDLE_KEY_MSG(EntryShutdown, "entry.shutdown.shortcut")
WORDLE_KEY_MSG(HintInput, "hint.input.shortcut")
WORDLE_KEY_MSG(EntryView, "entry.view.shortcut")
WORDLE_KEY_MSG(EntryFilter, "entry.filter.shortcut")
WORDLE_KEY_MSG(EntryBack, "entry.back.shortcut")
WORDLE_KEY_MSG(EntryCleanup, "entry.cleanup.shortcut")
WORDLE_KEY_MSG(SlotAddRestriction, "slot.add_restriction.shortcut")
WORDLE_KEY_MSG(EntryCopy, "entry.copy.shortcut")
WORDLE_KEY_MSG(EntrySearchWith, "entry.search_with.shortcut")

#undef WORDLE_KEY_MSG

inline std::string tr(const msg::SearchWith& v) {
    return I18n::active().render("template.search_with.shortcut", {{"engine", v.engine}, {"key", v.key}});
}

inline std::string tr(const msg::TermResize& v) {
    return I18n::active().render("terminal.resize", {{"width", v.width}, {"height", v.height}});
}
