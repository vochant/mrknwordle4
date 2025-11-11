#include "program/program.hpp"
#include "options.hpp"

#include <fstream>

std::map<std::string, std::shared_ptr<Environment>> envmap;

std::shared_ptr<Environment> getEnvironmentForModule(const std::string& moduleName) {
    if (!options->pluginIsolated) return gVM->inner;
    if (!envmap.count(moduleName)) {
        envmap.insert({moduleName, std::make_shared<Environment>(gVM->outer)});
    }
    return envmap[moduleName];
}

void Program::loadLibrary(std::shared_ptr<Plugin> _plg) {
    _plg->attach(_outer);
}

int Program::Execute(std::shared_ptr<ProgramNode> _program) {
	return gVM->Execute(_program, gVM->inner);
}

int Program::ExecuteCode(std::string src, std::string from) {
    Parser parser(src, from);
    auto prog = parser.parse_program();
    return gVM->Execute(std::dynamic_pointer_cast<ProgramNode>(prog), getEnvironmentForModule(from));
}

int Program::ExecuteOuter(std::shared_ptr<ProgramNode> _program) {
	return gVM->Execute(_program, gVM->outer);
}

Program::Program() {
    std::ios::sync_with_stdio(false);
    _outer = std::make_shared<CommonEnvironment>();
    gVM = new VirtualMachine(_outer);
}

Program::~Program() {
    delete gVM;
}