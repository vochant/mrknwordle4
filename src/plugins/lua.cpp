#include "plugins/runtime.hpp"
#include "dictReader.hpp"
#include "logger.hpp"
#include <lua.hpp>
#include <cstdlib>
#include <stdexcept>
#include <mutex>
#include <random>
#include <cstdint>
#include <cstdio>
#include <nlohmann/json.hpp>

#if LUA_VERSION_NUM < 504
#error Lua 5.4 or newer is required
#endif

namespace {
    class LuaRuntime final : public Runtime {
        static constexpr size_t memlimit = 8 * 1024 * 1024;
        size_t allocated = 0;
        lua_State* state = nullptr;
        int exports = LUA_NOREF;
        int loaders = LUA_NOREF, loaded = LUA_NOREF, loading = LUA_NOREF;
        std::map<std::string, std::string> modules;
        std::map<std::string, int> graderRefs;
        LuaRuntimeOptions options;
        ActivePluginResources resources;
        std::set<std::string> registeredGraders, registeredDictionaries, registeredWords;
        std::vector<int> retainedFunctions;
        bool initializing = true;
        bool failed = false;
        std::mutex mutex;
        static uint64_t newSeed() {
            std::random_device device;
            return (uint64_t(device()) << 32) ^ device();
        }

        static void* allocate(void* opaque, void* pointer, size_t oldSize, size_t newSize) {
            auto& self = *(LuaRuntime*) opaque;
            if (!pointer) oldSize = 0;
            if (!newSize) {
                self.allocated -= oldSize;
                std::free(pointer);
                return nullptr;
            }
            if (newSize > memlimit || self.allocated - oldSize > memlimit - newSize) return nullptr;
            void* replacement = std::realloc(pointer, newSize);
            if (replacement) self.allocated = self.allocated - oldSize + newSize;
            return replacement;
        }

        static int initialize(lua_State* state) {
            auto* self = *(LuaRuntime**) lua_getextraspace(state);
            luaL_requiref(state, "_G", luaopen_base, 1);
            lua_pop(state, 1);
            for (const char* name : {
                "collectgarbage", "dofile", "load", "loadfile",
                "print", "warn", "pcall", "xpcall"
            }) {
                lua_pushnil(state);
                lua_setglobal(state, name);
            }
            luaL_requiref(state, LUA_TABLIBNAME, luaopen_table, 1);
            lua_pop(state, 1);
            luaL_requiref(state, LUA_STRLIBNAME, luaopen_string, 1);
            lua_pop(state, 1);
            luaL_requiref(state, LUA_MATHLIBNAME, luaopen_math, 1);
            lua_pop(state, 1);
            luaL_requiref(state, LUA_IOLIBNAME, luaopen_io, 0);
            luaL_getmetatable(state, LUA_FILEHANDLE);
            lua_getfield(state, -1, "__index");
            for (const char* name : {"write", "flush", "setvbuf"}) {
                lua_pushnil(state);
                lua_setfield(state, -2, name);
            }
            lua_pop(state, 1);
            lua_pop(state, 1);
            lua_getfield(state, -1, "type");
            lua_newtable(state);
            lua_pushlightuserdata(state, self);
            lua_pushcclosure(state, hostOpen, 1);
            lua_setfield(state, -2, "open");
            lua_pushlightuserdata(state, self);
            lua_pushcclosure(state, hostLines, 1);
            lua_setfield(state, -2, "lines");
            lua_pushvalue(state, -2);
            lua_setfield(state, -2, "type");
            lua_setglobal(state, "io");
            lua_pop(state, 2);
            lua_newtable(state);
            lua_pushlightuserdata(state, self);
            lua_pushcclosure(state, hostReadDict, 1);
            lua_setfield(state, -2, "read_dict");
            lua_pushlightuserdata(state, self);
            lua_pushcclosure(state, hostLog, 1);
            lua_setfield(state, -2, "log");
            lua_newtable(state);
            lua_pushlightuserdata(state, self);
            lua_pushcclosure(state, hostRegisterGrader, 1);
            lua_setfield(state, -2, "grader");
            lua_pushlightuserdata(state, self);
            lua_pushcclosure(state, hostRegisterDictionary, 1);
            lua_setfield(state, -2, "dictionary");
            lua_pushlightuserdata(state, self);
            lua_pushcclosure(state, hostRegisterWords, 1);
            lua_setfield(state, -2, "words");
            lua_setfield(state, -2, "register");
            lua_setglobal(state, "plugin");
            lua_newtable(state);
            self->loaded = luaL_ref(state, LUA_REGISTRYINDEX);
            lua_newtable(state);
            self->loading = luaL_ref(state, LUA_REGISTRYINDEX);
            lua_newtable(state);
            for (const auto& mod : self->modules) {
                lua_pushlstring(state, mod.first.data(), mod.first.size());
                if (luaL_loadbufferx(
                    state,
                    mod.second.data(), mod.second.size(),
                    mod.first.c_str(), "t"
                ) != LUA_OK) {
                    return lua_error(state);
                }
                lua_rawset(state, -3);
            }
            self->loaders = luaL_ref(state, LUA_REGISTRYINDEX);
            lua_pushcfunction(state, requireModule);
            lua_setglobal(state, "require");
            return 0;
        }

        static int requireModule(lua_State* state) {
            auto* self = *(LuaRuntime**) lua_getextraspace(state);
            luaL_checktype(state, 1, LUA_TSTRING);
            lua_settop(state, 1);
            lua_rawgeti(state, LUA_REGISTRYINDEX, self->loaded);
            lua_pushvalue(state, 1);
            lua_rawget(state, -2);
            if (!lua_isnil(state, -1)) return 1;
            lua_pop(state, 1);
            lua_rawgeti(state, LUA_REGISTRYINDEX, self->loading);
            lua_pushvalue(state, 1);
            lua_rawget(state, -2);
            if (lua_toboolean(state, -1)) return luaL_error(state, "Cyclic plugin require");
            lua_pop(state, 1);
            lua_pushvalue(state, 1);
            lua_pushboolean(state, 1);
            lua_rawset(state, -3);
            lua_rawgeti(state, LUA_REGISTRYINDEX, self->loaders);
            lua_pushvalue(state, 1);
            lua_rawget(state, -2);
            if (!lua_isfunction(state, -1)) return luaL_error(state, "Module is not declared in runtime.modules");
            lua_call(state, 0, 1);
            if (lua_isnil(state, -1)) {
                lua_pop(state, 1);
                lua_pushboolean(state, 1);
            }
            lua_pushvalue(state, 1);
            lua_pushvalue(state, -2);
            lua_rawset(state, 2);
            lua_pushvalue(state, 1);
            lua_pushnil(state);
            lua_rawset(state, 3);
            return 1;
        }

        static int retainExports(lua_State* state) {
            auto* self = *(LuaRuntime**) lua_getextraspace(state);
            if (lua_isnil(state, 1)) lua_newtable(state);
            else luaL_checktype(state, 1, LUA_TTABLE);
            self->exports = luaL_ref(state, LUA_REGISTRYINDEX);
            return 0;
        }

        static int closeFile(lua_State* state) {
            auto* stream = (luaL_Stream*) luaL_checkudata(state, 1, LUA_FILEHANDLE);
            if (stream->f) {
                std::fclose(stream->f);
                stream->f = nullptr;
            }
            stream->closef = nullptr;
            return 0;
        }

        static std::string fieldString(lua_State* state, int index, const char* name) {
            index = lua_absindex(state, index);
            lua_getfield(state, index, name);
            const auto value = luaL_checkstring(state, -1);
            std::string result(value);
            lua_pop(state, 1);
            return result;
        }

        static bool fieldBoolean(lua_State* state, int index, const char* name, bool fallback = false) {
            index = lua_absindex(state, index);
            lua_getfield(state, index, name);
            if (lua_isnil(state, -1)) {
                lua_pop(state, 1);
                return fallback;
            }
            if (!lua_isboolean(state, -1)) throw std::invalid_argument(std::string(name) + " must be boolean");
            const auto result = lua_toboolean(state, -1);
            lua_pop(state, 1);
            return result;
        }

        static nlohmann::json readStates(lua_State* state, int index) {
            index = lua_absindex(state, index);
            luaL_checktype(state, index, LUA_TTABLE);
            nlohmann::json result = nlohmann::json::object();
            lua_pushnil(state);
            while (lua_next(state, index)) {
                int id;
                if (lua_isinteger(state, -2)) id = (int) lua_tointeger(state, -2);
                else if (lua_isstring(state, -2)) {
                    size_t parsed = 0;
                    const auto key = std::string(lua_tostring(state, -2));
                    try {
                        id = std::stoi(key, &parsed);
                    }
                    catch (...) {
                        throw std::invalid_argument("Invalid grader state ID: " + key);
                    }
                    if (parsed != key.size() || std::to_string(id) != key) {
                        throw std::invalid_argument("Grader state ID must be a canonical integer from 0 to 255");
                    }
                }
                else throw std::invalid_argument("Grader states must use integer keys");
                if (id < 0 || id > 255) {
                    throw std::invalid_argument("Grader state ID must be an integer from 0 to 255");
                }
                const auto stateIndex = lua_absindex(state, -1);
                luaL_checktype(state, stateIndex, LUA_TTABLE);
                nlohmann::json definition;
                definition["color"] = fieldString(state, stateIndex, "color");
                definition["mode"] = fieldString(state, stateIndex, "mode");
                lua_getfield(state, stateIndex, "overrides");
                luaL_checktype(state, -1, LUA_TTABLE);
                nlohmann::json overrides = nlohmann::json::array();
                for (size_t i = 1; i <= lua_rawlen(state, -1); i++) {
                    lua_rawgeti(state, -1, (lua_Integer) i);
                    if (!lua_isinteger(state, -1)) throw std::invalid_argument("Grader state overrides must be integers");
                    overrides.push_back((int) lua_tointeger(state, -1));
                    lua_pop(state, 1);
                }
                lua_pop(state, 1);
                definition["overrides"] = std::move(overrides);
                result[std::to_string(id)] = std::move(definition);
                lua_pop(state, 1);
            }
            return result;
        }

        void authorize(
            const std::set<std::string>& allowed, std::set<std::string>& registered,
            const std::string& id, const char* kind
        ) {
            if (!initializing) throw std::invalid_argument("Plugin registration is only allowed during initialization");
            if (!allowed.count(id)) throw std::invalid_argument(std::string("Undeclared ") + kind + " registration: " + id);
            if (!registered.insert(id).second) {
                throw std::invalid_argument(std::string("Duplicate ") + kind + " registration: " + id);
            }
        }

        int retainFunction(lua_State* state, int index, const char* name, bool required) {
            index = lua_absindex(state, index);
            lua_getfield(state, index, name);
            if (lua_isnil(state, -1) && !required) {
                lua_pop(state, 1);
                return LUA_NOREF;
            }
            luaL_checktype(state, -1, LUA_TFUNCTION);
            const auto reference = luaL_ref(state, LUA_REGISTRYINDEX);
            retainedFunctions.push_back(reference);
            return reference;
        }

        int addGrader(lua_State* state) {
            const auto id = std::string(luaL_checkstring(state, 1));
            authorize(options.registerGraders, registeredGraders, id, "grader");
            luaL_checktype(state, 2, LUA_TTABLE);
            const auto definitionIndex = lua_absindex(state, 2);
            ActiveGrader grader;
            grader.id = id;
            grader.definition["name"] = fieldString(state, definitionIndex, "name");
            grader.definition["deterministic"] = fieldBoolean(state, definitionIndex, "deterministic");
            lua_getfield(state, definitionIndex, "states");
            grader.definition["states"] = readStates(state, -1);
            lua_pop(state, 1);
            const auto check = retainFunction(state, definitionIndex, "check", true);
            const auto compatible = retainFunction(state, definitionIndex, "compatible", false);
            const auto start = retainFunction(state, definitionIndex, "start", false);
            const auto finish = retainFunction(state, definitionIndex, "finish", false);
            const auto runtime = this;
            grader.check = [runtime, check](const std::string& guess, const std::string& answer) {
                return runtime->grade(check, guess, answer);
            };
            if (compatible != LUA_NOREF) {
                grader.compatible = [runtime, compatible](
                    const std::string& guess, const std::string& answer,
                    const std::vector<int>& feedback
                ) {
                    return runtime->compatible(compatible, guess, answer, feedback);
                };
            }
            if (start != LUA_NOREF) grader.start = [runtime, start] { runtime->lifecycle(start); };
            if (finish != LUA_NOREF) grader.finish = [runtime, finish] { runtime->lifecycle(finish); };
            resources.graders.push_back(std::move(grader));
            return 0;
        }

        int addDictionary(lua_State* state) {
            const auto id = std::string(luaL_checkstring(state, 1));
            authorize(options.registerDictionaries, registeredDictionaries, id, "dictionary");
            luaL_checktype(state, 2, LUA_TTABLE);
            const auto definitionIndex = lua_absindex(state, 2);
            nlohmann::json definition;
            definition["name"] = fieldString(state, definitionIndex, "name");
            lua_getfield(state, definitionIndex, "equivalents");
            if (lua_isnil(state, -1)) definition["equivalents"] = nlohmann::json::array();
            else {
                luaL_checktype(state, -1, LUA_TTABLE);
                auto& equivalents = definition["equivalents"] = nlohmann::json::array();
                for (size_t i = 1; i <= lua_rawlen(state, -1); i++) {
                    lua_rawgeti(state, -1, (lua_Integer) i);
                    equivalents.push_back(std::string(luaL_checkstring(state, -1)));
                    lua_pop(state, 1);
                }
            }
            lua_pop(state, 1);
            resources.dictionaries.push_back({id, parse_dict(definition)});
            return 0;
        }

        int addWords(lua_State* state) {
            const auto id = std::string(luaL_checkstring(state, 1));
            authorize(options.registerWords, registeredWords, id, "words");
            luaL_checktype(state, 2, LUA_TTABLE);
            const auto definitionIndex = lua_absindex(state, 2);
            WordRule rule;
            rule.filter = fieldString(state, definitionIndex, "filter");
            rule.target = fieldString(state, definitionIndex, "target");
            const auto file = fieldString(state, definitionIndex, "file");
            const auto type = fieldString(state, definitionIndex, "type");
            if (!options.readableFiles.count(file)) {
                throw std::invalid_argument("Words file is not declared readable: " + file);
            }
            const auto words = read_dict(options.readFile(file), type);
            rule.words.insert(words.begin(), words.end());
            resources.words.push_back({id, std::move(rule)});
            return 0;
        }

        static luaL_Stream* openFile(lua_State* state, LuaRuntime* self, const std::string& id, const std::string& mode) {
            if (mode != "r" && mode != "rt" && mode != "rb") {
                throw std::invalid_argument("Plugin files are read-only");
            }
            if (!self->options.readableFiles.count(id)) {
                throw std::invalid_argument("Undeclared readable file: " + id);
            }
            auto contents = self->options.readFile(id);
            auto* file = (luaL_Stream*) lua_newuserdatauv(state, sizeof(luaL_Stream), 0);
            file->f = std::tmpfile();
            file->closef = closeFile;
            if (!file->f) {
                file->closef = nullptr;
                throw std::runtime_error("Cannot create plugin file handle");
            }
            if (!contents.empty() && std::fwrite(contents.data(), 1, contents.size(), file->f) != contents.size()) {
                std::fclose(file->f);
                file->f = nullptr;
                file->closef = nullptr;
                throw std::runtime_error("Cannot initialize plugin file handle");
            }
            std::rewind(file->f);
            luaL_setmetatable(state, LUA_FILEHANDLE);
            return file;
        }

        static int hostOpen(lua_State* state) {
            auto* self = (LuaRuntime*) lua_touserdata(state, lua_upvalueindex(1));
            try {
                const auto id = std::string(luaL_checkstring(state, 1));
                const auto mode = std::string(luaL_optstring(state, 2, "r"));
                openFile(state, self, id, mode);
                return 1;
            }
            catch (const std::exception& error) {
                return luaL_error(state, "%s", error.what());
            }
        }

        static int nextLine(lua_State* state) {
            auto* file = (luaL_Stream*) luaL_checkudata(state, lua_upvalueindex(1), LUA_FILEHANDLE);
            if (!file->f) return 0;
            std::string line;
            int character;
            while ((character = std::fgetc(file->f)) != EOF) {
                if (character == '\n') break;
                line.push_back((char) character);
            }
            if (character == EOF && line.empty()) {
                std::fclose(file->f);
                file->f = nullptr;
                file->closef = nullptr;
                return 0;
            }
            if (!line.empty() && line.back() == '\r') line.pop_back();
            lua_pushlstring(state, line.data(), line.size());
            return 1;
        }

        static int hostLines(lua_State* state) {
            auto* self = (LuaRuntime*) lua_touserdata(state, lua_upvalueindex(1));
            try {
                const auto id = std::string(luaL_checkstring(state, 1));
                const auto mode = std::string(luaL_optstring(state, 2, "r"));
                openFile(state, self, id, mode);
                lua_pushcclosure(state, nextLine, 1);
                return 1;
            }
            catch (const std::exception& error) {
                return luaL_error(state, "%s", error.what());
            }
        }

        static int hostReadDict(lua_State* state) {
            auto* self = (LuaRuntime*) lua_touserdata(state, lua_upvalueindex(1));
            try {
                const auto id = std::string(luaL_checkstring(state, 1));
                const auto type = std::string(luaL_checkstring(state, 2));
                if (!self->options.readableFiles.count(id)) {
                    throw std::invalid_argument("Undeclared readable file: " + id);
                }
                const auto words = read_dict(self->options.readFile(id), type);
                lua_createtable(state, (int) words.size(), 0);
                for (size_t i = 0; i < words.size(); i++) {
                    lua_pushlstring(state, words[i].data(), words[i].size());
                    lua_rawseti(state, -2, (lua_Integer) i + 1);
                }
                return 1;
            }
            catch (const std::exception& error) {
                return luaL_error(state, "%s", error.what());
            }
        }

        static int hostLog(lua_State* state) {
            auto* self = (LuaRuntime*) lua_touserdata(state, lua_upvalueindex(1));
            try {
                if (!self->options.allowLog) throw std::invalid_argument("Plugin logging permission not granted");
                const auto level = std::string(luaL_checkstring(state, 1));
                const auto message = std::string(luaL_checkstring(state, 2));
                if (message.size() > 4096) throw std::invalid_argument("Plugin log message exceeds 4 KiB");
                Logger::Level logLevel;
                if (level == "debug") logLevel = Logger::Debug;
                else if (level == "info") logLevel = Logger::Info;
                else if (level == "warn") logLevel = Logger::Warn;
                else if (level == "error") logLevel = Logger::Error;
                else throw std::invalid_argument("Unknown plugin log level");
                logger.write(logLevel, "PLUGIN:" + self->options.pluginId, message);
                return 0;
            }
            catch (const std::exception& error) {
                return luaL_error(state, "%s", error.what());
            }
        }

        static int hostRegisterGrader(lua_State* state) {
            auto* self = (LuaRuntime*) lua_touserdata(state, lua_upvalueindex(1));
            try { return self->addGrader(state); }
            catch (const std::exception& error) { return luaL_error(state, "%s", error.what()); }
        }

        static int hostRegisterDictionary(lua_State* state) {
            auto* self = (LuaRuntime*) lua_touserdata(state, lua_upvalueindex(1));
            try { return self->addDictionary(state); }
            catch (const std::exception& error) { return luaL_error(state, "%s", error.what()); }
        }

        static int hostRegisterWords(lua_State* state) {
            auto* self = (LuaRuntime*) lua_touserdata(state, lua_upvalueindex(1));
            try { return self->addWords(state); }
            catch (const std::exception& error) { return luaL_error(state, "%s", error.what()); }
        }

        struct Invocation {
            LuaRuntime* runtime;
            const std::string* name;
            const std::string* guess;
            const std::string* answer;
            std::vector<int>* states;
            const std::vector<int>* feedback;
            bool* compatible;
            int operation;
            int function = LUA_NOREF;
        };

        static int invoke(lua_State* state) {
            auto* invocation = (Invocation*) lua_touserdata(state, 1);
            if (invocation->function == LUA_NOREF) {
                lua_rawgeti(state, LUA_REGISTRYINDEX, invocation->runtime->exports);
                lua_pushlstring(state, invocation->name->data(), invocation->name->size());
                lua_rawget(state, -2);
            }
            else lua_rawgeti(state, LUA_REGISTRYINDEX, invocation->function);
            if (!lua_isfunction(state, -1)) { return luaL_error(state, "Missing rule export"); }
            if (invocation->operation == 1) {
                lua_call(state, 0, 0);
                return 0;
            }
            if (invocation->operation == 2) {
                lua_pushlstring(state, invocation->guess->data(), invocation->guess->size());
                lua_pushlstring(state, invocation->answer->data(), invocation->answer->size());
                lua_newtable(state);
                for (size_t i = 0; i < invocation->feedback->size(); i++) {
                    lua_pushinteger(state, (*invocation->feedback)[i]);
                    lua_rawseti(state, -2, lua_Integer(i + 1));
                }
                lua_call(state, 3, 1);
                *invocation->compatible = lua_toboolean(state, -1);
                return 0;
            }
            if (!invocation->guess) return 0;
            lua_pushlstring(state, invocation->guess->data(), invocation->guess->size());
            lua_pushlstring(state, invocation->answer->data(), invocation->answer->size());
            lua_call(state, 2, 1);
            if (!lua_istable(state, -1) || lua_rawlen(state, -1) != 5) {
                return luaL_error(state, "Grader must return five state IDs");
            }
            invocation->states->clear();
            for (int i = 1; i <= 5; i++) {
                lua_rawgeti(state, -1, i);
                if (!lua_isinteger(state, -1)) return luaL_error(state, "Grader state IDs must be integers");
                auto res = lua_tointeger(state, -1);
                if (res < 0 || res > 255) return luaL_error(state, "Grader state ID outside 0..255");
                invocation->states->push_back((int) res);
                lua_pop(state, 1);
            }
            return 1;
        }

        void check(int status) {
            if (status == LUA_OK) return;
            failed = true;
            const char* message = lua_type(state, -1) == LUA_TSTRING ? lua_tostring(state, -1) : "Lua plugin failed";
            std::string error = message ? message : "Lua plugin failed";
            lua_settop(state, 0);
            throw std::runtime_error(error);
        }

        void run(Invocation& invocation, int results) {
            if (failed) throw std::runtime_error("Lua plugin is disabled after a previous failure");
            lua_settop(state, 0);
            lua_pushcfunction(state, invoke);
            lua_pushlightuserdata(state, &invocation);
            check(lua_pcall(state, 1, results, 0));
        }

        void validate(const std::string& name) {
            Invocation invocation {this, &name, nullptr, nullptr, nullptr, nullptr, nullptr, 0};
            run(invocation, 0);
        }

    public:
        LuaRuntime(
            const std::string& source, const std::map<std::string, std::string>& modules,
            const LuaRuntimeOptions& options
        ) : modules(modules), options(options) {
#if LUA_VERSION_NUM >= 505
            state = lua_newstate(allocate, this, 0);
#else
            state = lua_newstate(allocate, this);
#endif
            if (!state) throw std::runtime_error("Cannot allocate Lua state");
            *(LuaRuntime**) lua_getextraspace(state) = this;
            try {
                lua_pushcfunction(state, initialize);
                check(lua_pcall(state, 0, 0, 0));
                this->modules.clear();
                check(luaL_loadbufferx(state, source.data(), source.size(), "plugin", "t"));
                check(lua_pcall(state, 0, 1, 0));
                lua_pushcfunction(state, retainExports);
                lua_insert(state, -2);
                check(lua_pcall(state, 1, 0, 0));
                initializing = false;
            }
            catch (...) {
                lua_close(state);
                state = nullptr;
                throw;
            }
        }

        ~LuaRuntime() override {
            if (state) lua_close(state);
        }

        ActivePluginResources takeRegistrations() override {
            return std::move(resources);
        }
        
        void validateGrader(const std::string& name) override {
            std::lock_guard<std::mutex> guard(mutex);
            validate(name);
        }

        void validateCompatible(const std::string& name) override {
            std::lock_guard<std::mutex> guard(mutex);
            validate(name);
        }

        void validateLifecycle(const std::string& name) override {
            std::lock_guard<std::mutex> guard(mutex);
            validate(name);
        }

        GraderFunc bindGrader(const std::string& name) override {
            std::lock_guard<std::mutex> guard(mutex);
            if (failed) throw std::runtime_error("Lua plugin is disabled after a previous failure");
            auto found = graderRefs.find(name);
            if (found == graderRefs.end()) {
                lua_settop(state, 0);
                lua_rawgeti(state, LUA_REGISTRYINDEX, exports);
                lua_pushlstring(state, name.data(), name.size());
                lua_rawget(state, -2);
                if (!lua_isfunction(state, -1)) {
                    lua_settop(state, 0);
                    throw std::runtime_error("Missing rule export");
                }
                const auto reference = luaL_ref(state, LUA_REGISTRYINDEX);
                lua_settop(state, 0);
                found = graderRefs.emplace(name, reference).first;
            }
            const auto runtime = this;
            const auto function = found->second;
            return [runtime, function](const std::string& guess, const std::string& answer) {
                return runtime->grade(function, guess, answer);
            };
        }

        std::vector<int> grade(const std::string& name, const std::string& guess, const std::string& answer) override {
            std::lock_guard<std::mutex> guard(mutex);
            std::vector<int> states;
            Invocation invocation {this, &name, &guess, &answer, &states, nullptr, nullptr, 0};
            run(invocation, 1);
            lua_settop(state, 0);
            return states;
        }

        std::vector<int> grade(int function, const std::string& guess, const std::string& answer) {
            std::lock_guard<std::mutex> guard(mutex);
            std::vector<int> states;
            Invocation invocation {this, nullptr, &guess, &answer, &states, nullptr, nullptr, 0, function};
            run(invocation, 1);
            lua_settop(state, 0);
            return states;
        }

        void lifecycle(const std::string& exportName) {
            std::lock_guard<std::mutex> guard(mutex);
            Invocation invocation {this, &exportName, nullptr, nullptr, nullptr, nullptr, nullptr, 1};
            run(invocation, 0);
        }

        void lifecycle(int function) {
            std::lock_guard<std::mutex> guard(mutex);
            Invocation invocation {this, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 1, function};
            run(invocation, 0);
        }

        void start(const std::string& name) override {
            std::lock_guard<std::mutex> guard(mutex);
            const uint64_t seed = newSeed();
            lua_settop(state, 0);
            lua_getglobal(state, "math");
            lua_getfield(state, -1, "randomseed");
            lua_pushinteger(state, lua_Integer(seed >> 32));
            lua_pushinteger(state, lua_Integer(seed & 0xffffffffULL));
            check(lua_pcall(state, 2, 0, 0));
            lua_settop(state, 0);
            Invocation invocation {this, &name, nullptr, nullptr, nullptr, nullptr, nullptr, 1};
            run(invocation, 0);
        }

        void finish(const std::string& name) override {
            lifecycle(name);
        }

        bool compatible(
            const std::string& name, const std::string& guess, const std::string& answer,
            const std::vector<int>& feedback
        ) override {
            std::lock_guard<std::mutex> guard(mutex);
            bool result = false;
            Invocation invocation {this, &name, &guess, &answer, nullptr, &feedback, &result, 2};
            run(invocation, 0);
            return result;
        }

        bool compatible(
            int function, const std::string& guess, const std::string& answer,
            const std::vector<int>& feedback
        ) {
            std::lock_guard<std::mutex> guard(mutex);
            bool result = false;
            Invocation invocation {this, nullptr, &guess, &answer, nullptr, &feedback, &result, 2, function};
            run(invocation, 0);
            return result;
        }
    };
} // namespace

std::shared_ptr<Runtime> create_lua_runtime(
    const std::string& source,
    const std::map<std::string, std::string>& modules, const LuaRuntimeOptions& options
) {
    return std::make_shared<LuaRuntime>(source, modules, options);
}
