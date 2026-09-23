#include "plugins/runtime.hpp"
#include <lua.hpp>
#include <cstdlib>
#include <stdexcept>
#include <mutex>
#include <random>
#include <cstdint>

#if LUA_VERSION_NUM < 504
#error Lua 5.4 or newer is required
#endif

namespace {
    class LuaRuntime final : public Runtime {
        static constexpr size_t memlimit = 8 * 1024 * 1024;
        static constexpr int inslimit = 100000;
        size_t allocated = 0;
        int budget = 0;
        lua_State* state = nullptr;
        int exports = LUA_NOREF;
        int loaders = LUA_NOREF, loaded = LUA_NOREF, loading = LUA_NOREF;
        std::map<std::string, std::string> modules;
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

        static void interrupt(lua_State* state, lua_Debug*) {
            auto* self = *(LuaRuntime**) lua_getextraspace(state);
            self->budget -= 1000;
            if (self->budget <= 0) luaL_error(state, "Plugin instruction budget exceeded");
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
            for (const char* name : {"rep", "find", "match", "gmatch", "gsub", "dump"}) {
                lua_getglobal(state, "string");
                lua_pushnil(state);
                lua_setfield(state, -2, name);
                lua_pop(state, 1);
            }
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
            luaL_checktype(state, 1, LUA_TTABLE);
            self->exports = luaL_ref(state, LUA_REGISTRYINDEX);
            return 0;
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
        };

        static int invoke(lua_State* state) {
            auto* invocation = (Invocation*) lua_touserdata(state, 1);
            lua_rawgeti(state, LUA_REGISTRYINDEX, invocation->runtime->exports);
            lua_pushlstring(state, invocation->name->data(), invocation->name->size());
            lua_rawget(state, -2);
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
            budget = inslimit;
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
        LuaRuntime(const std::string& source, const std::map<std::string, std::string>& modules) : modules(modules) {
#if LUA_VERSION_NUM >= 505
            state = lua_newstate(allocate, this, 0);
#else
            state = lua_newstate(allocate, this);
#endif
            if (!state) throw std::runtime_error("Cannot allocate Lua state");
            *(LuaRuntime**) lua_getextraspace(state) = this;
            lua_sethook(state, interrupt, LUA_MASKCOUNT, 1000);
            budget = inslimit;
            try {
                lua_pushcfunction(state, initialize);
                check(lua_pcall(state, 0, 0, 0));
                this->modules.clear();
                check(luaL_loadbufferx(state, source.data(), source.size(), "plugin", "t"));
                check(lua_pcall(state, 0, 1, 0));
                lua_pushcfunction(state, retainExports);
                lua_insert(state, -2);
                check(lua_pcall(state, 1, 0, 0));
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

        std::vector<int> grade(const std::string& name, const std::string& guess, const std::string& answer) override {
            std::lock_guard<std::mutex> guard(mutex);
            std::vector<int> states;
            Invocation invocation {this, &name, &guess, &answer, &states, nullptr, nullptr, 0};
            run(invocation, 1);
            lua_settop(state, 0);
            return states;
        }

        void lifecycle(const std::string& exportName) {
            std::lock_guard<std::mutex> guard(mutex);
            Invocation invocation {this, &exportName, nullptr, nullptr, nullptr, nullptr, nullptr, 1};
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
    };
} // namespace

std::shared_ptr<Runtime> create_lua_runtime(
    const std::string& source,
    const std::map<std::string, std::string>& modules
) {
    return std::make_shared<LuaRuntime>(source, modules);
}
