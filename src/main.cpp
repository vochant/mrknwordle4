#include "options.hpp"
#include "userdb.hpp"
#include "char.hpp"
#include "terminal/session.hpp"
#include "ui/application.hpp"
#include "registry.hpp"
#include "plugins/manager.hpp"
#include "i18n.hpp"
#include "locale.hpp"
#include "logger.hpp"
#include <nlohmann/json.hpp>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <exception>
#include <fstream>
#include <memory>
#include <string>

namespace {
    std::string program_name(const char* path) {
        if (path == nullptr || *path == '\0') return "wordle4";
        std::string name(path);
        auto slash = name.find_last_of("/\\");
        if (slash != std::string::npos) name.erase(0, slash + 1);
        return name.empty() ? "wordle4" : name;
    }

    void help(const std::string& program) {
        std::printf(
            "Usage: %s [OPTION]\n"
            "\n"
            "Run the interactive mrknwordle4 terminal interface.\n"
            "\n"
            "With no OPTION, runtime data are loaded from the current working\n"
            "directory and the interactive application is started.\n"
            "\n"
            "Options:\n"
            "  -h, --help     display this help and exit\n"
            "  -a, --about    display program information and exit\n"
            "  -v, --version  output version information and exit\n"
            "\n"
            "Files:\n"
            "  config.json  main runtime configuration\n"
            "  core.json    required core resource manifest\n"
            "  i18n/        localization catalogs\n"
            "  user.db      persistent user database\n"
            "  logs/        diagnostic logs\n"
            "\n"
            "Dictionary and plugin files are loaded from paths declared by the\n"
            "configuration and resource manifests.\n"
            "\n"
            "Exit status:\n"
            "  0  successful completion\n"
            "  1  startup or runtime error\n"
            "  2  command-line usage error\n",
            program.c_str()
        );
    }

    void about() {
        std::printf(
            "mrknwordle4 %s\n"
            "A configurable terminal implementation of Wordle.\n"
            "\n"
            "Provides an interactive terminal interface with configurable dictionaries\n"
            "and graders, localization, persistent user data, and optional plugins.\n"
            "Runtime data are loaded relative to the current working directory.\n"
            "\n"
            "MIT License\n"
            "\n"
            "Copyright (c) 2026 Mirekintoc Void\n"
            "\n"
            "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
            "of this software and associated documentation files (the \"Software\"), to deal\n"
            "in the Software without restriction, including without limitation the rights\n"
            "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
            "copies of the Software, and to permit persons to whom the Software is\n"
            "furnished to do so, subject to the following conditions:\n"
            "\n"
            "The above copyright notice and this permission notice shall be included in all\n"
            "copies or substantial portions of the Software.\n"
            "\n"
            "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
            "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
            "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
            "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
            "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
            "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
            "SOFTWARE.\n",
            WORDLE_VERSION
        );
    }
} // namespace

int main(int argc, char** argv) try {
    const auto program = program_name(argc > 0 ? argv[0] : nullptr);
    if (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        help(program);
        return 0;
    }
    if (argc == 2 && (std::string(argv[1]) == "-a" || std::string(argv[1]) == "--about")) {
        about();
        return 0;
    }
    if (argc == 2 && (std::string(argv[1]) == "-v" || std::string(argv[1]) == "--version")) {
        std::printf("%s %s\n", program.c_str(), WORDLE_VERSION);
        return 0;
    }
    if (argc != 1) {
        if (argc == 2) {
            const char* argument = argv[1];
            std::fprintf(
                stderr, "%s: %s '%s'\n",
                program.c_str(), argument[0] == '-' ? "unrecognized option" : "unexpected operand", argument
            );
        }
        else std::fprintf(stderr, "%s: too many arguments\n", program.c_str());
        std::fprintf(stderr, "Try '%s --help' for more information.\n", program.c_str());
        return 2;
    }
    errno = 0;
    std::ifstream ifs("config.json");
    if (!ifs) {
        const int error = errno;
        if (error) std::fprintf(stderr, "%s: config.json: %s\n", program.c_str(), std::strerror(error));
        else std::fprintf(stderr, "%s: config.json: cannot open for reading\n", program.c_str());
        return 1;
    }
    nlohmann::json json_config;
    std::unique_ptr<Options> config;
    try {
        ifs >> json_config;
        config = std::make_unique<Options>(json_config);
    }
    catch (const std::exception& error) {
        std::fprintf(stderr, "%s: config.json: %s\n", program.c_str(), error.what());
        return 1;
    }
    options = config.get();
    logger.write(Logger::Info, "INIT", "已加载配置");
    init_text_locale();
    I18n language("i18n", options->locale);
    I18n::Binding languageBinding(language);
    Registry res;
    registry = &res;
    PluginManager plugins(*options, res);
    pluginManager = &plugins;
    plugins.load();
    res.validateConfig(*options);
    logger.write(Logger::Info, "CHECK", "已完成资源和配置检查");
    struct DatabaseCleanup {
        ~DatabaseCleanup() { close_user_db(); }
    } databaseCleanup;
    if (!init_user_db()) {
        std::fprintf(stderr, "%s: %s\n", program.c_str(), tr(Msg::ErrorDatabase).c_str());
        return 1;
    }
    logger.write(Logger::Info, "INIT", "数据库已初始化");
    TermSession session;
    logger.start();
    run_app();
    return 0;
}
catch (const std::exception& error) {
    logger.write(Logger::Error, "MAIN", error.what());
    const auto program = program_name(argc > 0 ? argv[0] : nullptr);
    std::fprintf(stderr, "%s: fatal: %s\n", program.c_str(), error.what());
    return 1;
}
