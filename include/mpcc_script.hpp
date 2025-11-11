#pragma once

#include "program/program.hpp"
#include "plugins/plugin.hpp"
#include "logger.hpp"
#include <exception>

Program* program;

void ScriptInit() {
    program = new Program();
    program->loadLibrary(std::make_shared<Plugins::Base>());
    program->loadLibrary(std::make_shared<Plugins::IO>());
    program->loadLibrary(std::make_shared<Plugins::FileIO>());
    program->loadLibrary(std::make_shared<Plugins::Math>());
    program->loadLibrary(std::make_shared<Plugins::Wordle>());
    program->loadLibrary(std::make_shared<Plugins::DynamicLoad>());
}

void RunScript(std::string buf, std::string src = "<unknown>") {
    try {
        program->ExecuteCode(buf, src);
    }
    catch (const std::exception& e) {
        logger.write(Logger::Error, src, "运行脚本时出错：" + std::string(e.what()));
    }
}