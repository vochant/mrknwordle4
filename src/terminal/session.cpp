#include "terminal/session.hpp"
#include "terminal/terminal.hpp"
#include "logger.hpp"
#include "options.hpp"

#include <exception>
#include <string>

TermSession::TermSession() : handlesInterrupts(options->term.handleInterrupt) {
    if (handlesInterrupts) {
        logger.write(Logger::Debug, "INIT", "安装终端信号处理");
        install_term_signals();
    }
    try {
        logger.write(Logger::Debug, "INIT", "初始化终端后端: " + options->term.backend);
        term = create_term(options->term);
        term->setBackground(options->colors.background.value);
        term->setTitle("MrknWordle " WORDLE_VERSION);
        termScreen.setBackground(options->colors.background.value);
        auto dimensions = term->size();
        termScreen.resize(dimensions.first, dimensions.second);
        logger.write(
            Logger::Info, "INIT",
            "终端已初始化: " + std::to_string(dimensions.first) + "x" + std::to_string(dimensions.second)
        );
    }
    catch (const std::exception& error) {
        logger.write(Logger::Error, "INIT", "终端初始化失败: " + std::string(error.what()));
        term.reset();
        if (handlesInterrupts) restore_term_signals();
        throw;
    }
}

TermSession::~TermSession() {
    logger.write(Logger::Debug, "EXIT", "关闭终端后端");
    term.reset();
    if (handlesInterrupts) {
        logger.write(Logger::Debug, "EXIT", "恢复终端信号处理");
        restore_term_signals();
    }
}
