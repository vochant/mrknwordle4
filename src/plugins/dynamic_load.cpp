#include "plugins/plugin.hpp"
#include "vm_error.hpp"
#include "object/string.hpp"
#include "options.hpp"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <filesystem>
#include <map>

std::map<std::string, HMODULE> loadeds;

typedef std::shared_ptr<Object> (*base_function_t)(Args);

std::shared_ptr<NativeFunction> get_ext_function(const std::string _library, const std::string _symbol) {
    HMODULE _module_handle;
    if (loadeds.count(_library)) {
        _module_handle = loadeds[_library];
    }
    else {
        _module_handle = LoadLibraryA(_library.c_str());
        if (_module_handle == NULL) {
            throw VMError("native:get", "Failed to load library: " + _library);
        }
        loadeds.insert({_library, _module_handle});
    }
    
    NFunc func = (base_function_t)GetProcAddress(_module_handle, _symbol.c_str());
    if (func == NULL) {
        throw VMError("native:get", "Failed to find symbol: " + _symbol + " (in library " + _library + ")");
    }
    return std::make_shared<NativeFunction>(func);
}

Plugins::DynamicLoad::DynamicLoad() {}

std::shared_ptr<Object> DL_LoadLibrary(Args args) {
    if (!options->pluginDynamicLibraries) {
        throw VMError("(DynamicLoad)DL_LoadLibrary", "Dynamic library loading is disabled by the config");
    }
    plain(args);
    if (args.size() != 2 || args[0]->type != Object::Type::String || args[1]->type != Object::Type::String) {
        throw VMError("(DynamicLoad)DL_LoadLibrary", "Incorrect Format");
    }
    return get_ext_function(std::dynamic_pointer_cast<String>(args[0])->value, std::dynamic_pointer_cast<String>(args[1])->value);
}

void Plugins::DynamicLoad::enable() {
    regist("load", DL_LoadLibrary);
} 