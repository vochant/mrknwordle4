#pragma once

#include <cstdint>
#include <string>

enum class Msg {
    FilterHelp,
    ErrorClipboard,
    ErrorBrowser,
    PluginLoaded,
    PluginFailed,
    ErrorDatabase,
    HintInvalidResult,
    HintInvalidHistory,
    HintFailedToGuess,
    SetupConfirm,
    SetupHelp,
    SetupAdvanced,
    SetupBack,
    PrevPage,
    NextPage,
    HintNotLogged,
    SlotUsername,
    SlotPassword,
    ErrorUsernameEmpty,
    ErrorPasswordEmpty,
    ErrorSystemError,
    ErrorIncorrectCredentials,
    SlotRepeatPassword,
    ErrorPasswordMismatch,
    ErrorUserExists,
    SlotOriginalPassword,
    NewUsernameLabel,
    NewPasswordLabel,
    RepeatPasswordlLabel,
    HintDeleteAccount,
    SetupOn,
    SetupOff,
    SetupGame,
    SetupDictionary,
    SetupAnswerPool,
    SetupAnswers,
    SetupAcceptable,
    SetupValidate,
    SetupLimit,
    SetupAlphabet,
    SetupViewer,
    SetupViewPool,
    SetupGrader,
    SetupImpossible,
    HintPluginsDisabled,
    HintHistory,
    HintPleaseInput,
    HintInvalidInput,
    HintEmptyHistory,
    HintWin,
    HintLose,
    SlotDeleteAllRestriction,
    HintNoScreen,
};

inline constexpr const char* msgIds[] = {
    "filter.help",
    "error.clipboard",
    "error.browser",
    "plugin.loaded",
    "plugin.failed",
    "error.database",
    "hint.invalid_result",
    "hint.invalid_history",
    "hint.failed_to_guess",
    "setup.confirm",
    "setup.help",
    "setup.advanced",
    "setup.back",
    "pagination.previous",
    "pagination.next",
    "hint.not_logged",
    "slot.username",
    "slot.password",
    "error.username_empty",
    "error.password_empty",
    "error.system_error",
    "error.incorrect_credentials",
    "slot.repeat_password",
    "error.password_mismatch",
    "error.user_exists",
    "slot.original_password",
    "slot.new_username.hint.optional.label",
    "slot.new_password.hint.optional.label",
    "slot.repeat_password.hint.optional.label",
    "hint.delete_account",
    "setup.on",
    "setup.off",
    "setup.game",
    "setup.dictionary",
    "setup.answerPool",
    "setup.answers",
    "setup.acceptable",
    "setup.validate",
    "setup.limit",
    "setup.alphabet",
    "setup.viewer",
    "setup.viewPool",
    "setup.grader",
    "setup.impossible",
    "hint.plugins_disabled",
    "hint.history",
    "hint.please_input",
    "hint.invalid_input",
    "hint.empty_history",
    "hint.win",
    "hint.lose",
    "slot.delete_all_restriction",
    "hint.no_screen"
};

namespace msg {
    struct TemplateWelcome {
        std::string name;
    };
    struct SlotNumGuess {
        int64_t count;
    };
    struct FilterRemoveWord {
        std::string word;
    };
    struct ErrorGrader {
        std::string detail;
    };
    struct ActionConfirm {
        std::string key;
    };
    struct ActionCancel {
        std::string key;
    };
    struct ActionYes {
        std::string key;
    };
    struct ActionNo {
        std::string key;
    };
    struct Pagination {
        int64_t page;
        int64_t pages;
    };
    struct FieldValue {
        std::string label;
        std::string value;
    };
    struct PluginSummary {
        std::string name, id, version, status, author, license, url, description, diagnostics;
    };
    struct HistoryDetail {
        int64_t id;
        std::string date, dictionary, grader, answer, result;
    };
    struct HistoryEntry {
        int64_t id;
        std::string date, answer;
    };
    struct HintBack {
        std::string key;
    };
    struct EntryLogin {
        std::string key;
    };
    struct EntryRegister {
        std::string key;
    };
    struct EntrySearchSettings {
        std::string key;
    };
    struct EntrySecuritySettings {
        std::string key;
    };
    struct EntryLogout {
        std::string key;
    };
    struct EntryDeleteAccount {
        std::string key;
    };
    struct EntryGame {
        std::string key;
    };
    struct EntryHistory {
        std::string key;
    };
    struct EntryUser {
        std::string key;
    };
    struct EntryPlugins {
        std::string key;
    };
    struct EntryDictionary {
        std::string key;
    };
    struct EntryShutdown {
        std::string key;
    };
    struct HintInput {
        std::string key;
    };
    struct EntryView {
        std::string key;
    };
    struct EntryFilter {
        std::string key;
    };
    struct EntryBack {
        std::string key;
    };
    struct EntryCleanup {
        std::string key;
    };
    struct SlotAddRestriction {
        std::string key;
    };
    struct EntryCopy {
        std::string key;
    };
    struct SearchWith {
        std::string engine, key;
    };
    struct EntrySearchWith {
        std::string key;
    };
    struct TermResize {
        int64_t width, height;
    };
} // namespace msg
