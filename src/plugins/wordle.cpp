#include "plugins/plugin.hpp"
#include "object/nativeobject.hpp"
#include "object/integer.hpp"
#include "object/string.hpp"
#include "object/array.hpp"
#include "object/reference.hpp"

#include "vm/vm.hpp"

#include "options.hpp"

#include "vm_error.hpp"

Plugins::Wordle::Wordle() {}

std::vector<int> ScriptJudger(std::string str, std::string answer, std::shared_ptr<Executable> exec) {
    auto raw_res = exec->call({std::make_shared<String>(str), std::make_shared<String>(answer)});
    if (raw_res->type != Object::Type::Array) return {0, 0, 0, 0, 0};
    auto arr = std::dynamic_pointer_cast<Array>(raw_res);
    std::vector<int> result;
    for (auto item : arr->value) {
        if (item->type != Object::Type::Integer) result.push_back(0);
        else result.push_back((int) std::dynamic_pointer_cast<Integer>(item)->value);
    }
    return result;
}

bool ScriptValidator(std::string str, std::shared_ptr<Executable> exec) {
    auto raw_res = exec->call({std::make_shared<String>(str)});
    return gVM->isTrue(raw_res);
}

std::string ScriptProblemset(std::shared_ptr<Executable> exec) {
    auto raw_res = exec->call({});
    if (raw_res->type != Object::Type::String) {
        throw std::runtime_error("Plugin Defined Problemset: Invalid return type");
    }
    return std::dynamic_pointer_cast<String>(raw_res)->value;
}

std::shared_ptr<NativeObject> createWordleObject(std::string type) {
    auto ptr = std::make_shared<NativeObject>();
    ptr->set("type", std::make_shared<String>(type));
    ptr->set("__wordle__", std::make_shared<Integer>(8201));
    return ptr;
}

bool checkWordleObject(std::shared_ptr<Object> obj, std::string expectedType) {
    if (obj->type != Object::Type::CommonObject) return false;
    auto native = std::dynamic_pointer_cast<NativeObject>(obj);
    if (native->get("__wordle__")->type != Object::Type::Integer || std::dynamic_pointer_cast<Integer>(native->get("__wordle__"))->value != 8201) return false;
    return native->get("type")->type == Object::Type::String && std::dynamic_pointer_cast<String>(native->get("type"))->value == expectedType;
}

std::shared_ptr<Object> Wordle_InDictionary(Args args) {
    plain(args);
    if (args.size() != 2) {
        throw VMError("(Wordle)In_Dictionary", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::String || args[1]->type != Object::Type::String) {
        throw VMError("(Wordle)In_Dictionary", "Invalid argument types");
    }
    auto dict = std::dynamic_pointer_cast<String>(args[0])->value;
    auto word = std::dynamic_pointer_cast<String>(args[1])->value;

    if (!options->dictionaries.count(dict)) return gVM->False;
    return options->dictionaries[dict].count(word) ? gVM->True : gVM->False;
}

std::shared_ptr<Object> Wordle_SizeDictionary(Args args) {
    plain(args);
    if (args.size() != 1) {
        throw VMError("(Wordle)Size_Dictionary", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::String) {
        throw VMError("(Wordle)Size_Dictionary", "Invalid argument types");
    }
    auto dict = std::dynamic_pointer_cast<String>(args[0])->value;
    if (!options->dictionaries.count(dict)) return gVM->IntegerConstants[32];
    return std::make_shared<Integer>(options->dictionaries[dict].size());
}

std::shared_ptr<Object> Wordle_IndexDictionary(Args args) {
    plain(args);
    if (args.size() != 2) {
        throw VMError("(Wordle)Index_Dictionary", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::String || args[1]->type != Object::Type::Integer) {
        throw VMError("(Wordle)Index_Dictionary", "Invalid argument types");
    }
    auto dict = std::dynamic_pointer_cast<String>(args[0])->value;
    auto index = std::dynamic_pointer_cast<Integer>(args[1])->value;
    if (!options->dictionaries.count(dict)) return gVM->VNull;
    if (index < 0 || index >= options->dictionaries[dict].size()) return gVM->VNull;
    auto it = options->dictionaries[dict].begin();
    std::advance(it, index);
    return std::make_shared<String>(*it);
}

std::shared_ptr<Object> Wordle_CreateDictionary(Args args) {
    if (!(options->pluginFeatures & PLUGIN_DICTIONARIES)) {
        throw VMError("(Wordle)Create_Dictionary", "Plugin defined dictionaries are not enabled");
    }
    plain(args);
    if (args.size() != 1) {
        throw VMError("(Wordle)Create_Dictionary", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::String) {
        throw VMError("(Wordle)Create_Dictionary", "Invalid argument types");
    }
    auto dict = std::dynamic_pointer_cast<String>(args[0])->value;
    if (options->dictionaries.count(dict)) return gVM->False;
    options->dictionaries.insert({dict, {}});
    return gVM->True;
}

std::shared_ptr<Object> Wordle_AddWord(Args args) {
    if (!(options->pluginFeatures & PLUGIN_WORDS)) {
        throw VMError("(Wordle)Add_Word", "Plugin defined words are not enabled");
    }
    plain(args);
    if (args.size() != 2) {
        throw VMError("(Wordle)Add_Word", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::String || args[1]->type != Object::Type::String) {
        throw VMError("(Wordle)Add_Word", "Invalid argument types");
    }
    auto dict = std::dynamic_pointer_cast<String>(args[0])->value;
    auto word = std::dynamic_pointer_cast<String>(args[1])->value;
    if (!options->dictionaries.count(dict)) return gVM->False;
    if (!basicValidation(word)) return gVM->False;
    options->dictionaries[dict].insert(word);
    return gVM->True;
}

std::shared_ptr<Object> Wordle_CreateJudger(Args args) {
    if (!(options->pluginFeatures & PLUGIN_JUDGERS)) {
        throw VMError("(Wordle)Create_Judger", "Plugin defined judgers are not enabled");
    }
    plain(args);
    if (args.size() != 1) {
        throw VMError("(Wordle)Create_Judger", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::String) {
        throw VMError("(Wordle)Create_Judger", "Invalid argument types");
    }
    auto id = std::dynamic_pointer_cast<String>(args[0])->value;
    if (options->judgers.count(id)) return gVM->VNull;
    auto obj = createWordleObject("judger");
    obj->set("id", std::make_shared<String>(id));
    obj->set("determined", gVM->False);
    obj->set("states", std::make_shared<Array>());
    obj->set("exec", gVM->VNull);
    return obj;
}

std::shared_ptr<Object> Wordle_JudgerConfig(Args args) {
    if (!(options->pluginFeatures & PLUGIN_JUDGERS)) {
        throw VMError("(Wordle)Judger_Config", "Plugin defined judgers are not enabled");
    }
    if (args.size() != 3) {
        throw VMError("(Wordle)Judger_Config", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::Reference) {
        throw VMError("(Wordle)Judger_Config", "Invalid argument types");
    }
    args[0] = *std::dynamic_pointer_cast<Reference>(args[0])->ptr;
    plain(args);
    if (!checkWordleObject(args[0], "judger") || args[1]->type != Object::Type::String) {
        throw VMError("(Wordle)Judger_Config", "Invalid argument types");
    }
    auto obj = std::dynamic_pointer_cast<NativeObject>(args[0]);
    auto configItem = std::dynamic_pointer_cast<String>(args[1])->value;
    if (configItem == "determined") {
        if (args[2]->type != Object::Type::Boolean) {
            throw VMError("(Wordle)Judger_Config", "Invalid argument types");
        }
        obj->set("determined", args[2]);
    }
    else {
        throw VMError("(Wordle)Judger_Config", "Invalid item");
    }
    return gVM->VNull;
}

std::shared_ptr<Object> Wordle_JudgerResult(Args args) {
    if (!(options->pluginFeatures & PLUGIN_JUDGERS)) {
        throw VMError("(Wordle)Judger_Result", "Plugin defined judgers are not enabled");
    }
    if (args.size() != 5) {
        throw VMError("(Wordle)Judger_Result", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::Reference) {
        throw VMError("(Wordle)Judger_Result", "Invalid argument types");
    }
    args[0] = *std::dynamic_pointer_cast<Reference>(args[0])->ptr;
    plain(args);
    if (!checkWordleObject(args[0], "judger") || args[1]->type != Object::Type::Integer || args[2]->type != Object::Type::Integer || args[3]->type != Object::Type::Boolean || args[4]->type != Object::Type::Array) {
        throw VMError("(Wordle)Judger_Result", "Invalid argument types");
    }
    auto obj = std::dynamic_pointer_cast<NativeObject>(args[0]);
    int state = std::dynamic_pointer_cast<Integer>(args[1])->value;
    int color16 = std::dynamic_pointer_cast<Integer>(args[2])->value;
    bool mode = gVM->isTrue(args[3]);
    if (state < 0 || state > 255) {
        throw VMError("(Wordle)Judger_Result", "State must be between 0 and 255");
    }
    if (color16 < 0 || color16 > 15) {
        throw VMError("(Wordle)Judger_Result", "Color must be between 0 and 15");
    }
    auto _arr = std::dynamic_pointer_cast<Array>(args[4]);
    for (const auto& item : _arr->value) {
        if (item->type != Object::Type::Integer) {
            throw VMError("(Wordle)Judger_Result", "Invalid array element type");
        }
    }
    auto& states = std::dynamic_pointer_cast<Array>(obj->get("states"))->value;
    auto arr = std::make_shared<Array>();
    arr->value = { args[1], args[2], args[3], args[4] };
    states.push_back(arr);
    return gVM->VNull;
}

std::shared_ptr<Object> Wordle_JudgerSet(Args args) {
    if (!(options->pluginFeatures & PLUGIN_JUDGERS)) {
        throw VMError("(Wordle)Judger_Set", "Plugin defined judgers are not enabled");
    }
    if (args.size() != 2) {
        throw VMError("(Wordle)Judger_Set", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::Reference) {
        throw VMError("(Wordle)Judger_Set", "Invalid argument types");
    }
    args[0] = *std::dynamic_pointer_cast<Reference>(args[0])->ptr;
    plain(args);
    if (!checkWordleObject(args[0], "judger") || args[1]->type != Object::Type::Executable) {
        throw VMError("(Wordle)Judger_Set", "Invalid argument types");
    }
    auto obj = std::dynamic_pointer_cast<NativeObject>(args[0]);
    obj->set("exec", args[1]);
    return gVM->VNull;
}

std::shared_ptr<Object> Wordle_RegisterJudger(Args args) {
    if (!(options->pluginFeatures & PLUGIN_JUDGERS)) {
        throw VMError("(Wordle)Register_Judger", "Plugin defined judgers are not enabled");
    }
    if (args.size() != 1) {
        throw VMError("(Wordle)Register_Judger", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::Reference) {
        throw VMError("(Wordle)Register_Judger", "Invalid argument types");
    }
    args[0] = *std::dynamic_pointer_cast<Reference>(args[0])->ptr;
    plain(args);
    if (!checkWordleObject(args[0], "judger")) {
        throw VMError("(Wordle)Register_Judger", "Invalid argument types");
    }
    auto obj = std::dynamic_pointer_cast<NativeObject>(args[0]);
    if (obj->get("id")->type != Object::Type::String || obj->get("determined")->type != Object::Type::Boolean || obj->get("exec")->type != Object::Type::Executable || obj->get("states")->type != Object::Type::Array) {
        throw VMError("(Wordle)Register_Judger", "Invalid judger object");
    }
    auto id = std::dynamic_pointer_cast<String>(obj->get("id"))->value;
    if (options->judgers.count(id)) {
        throw VMError("(Wordle)Register_Judger", "Id " + id + " cannot be redefined");
    }
    JudgerType* judger = new JudgerType();
    judger->determined = gVM->isTrue(obj->get("determined"));
    judger->func = std::bind(ScriptJudger, std::placeholders::_1, std::placeholders::_2, std::dynamic_pointer_cast<Executable>(obj->get("exec")));
    auto& states = std::dynamic_pointer_cast<Array>(obj->get("states"))->value;
    for (auto& item : states) {
        if (item->type != Object::Type::Array) {
            delete judger;
            throw VMError("(Wordle)Register_Judger", "Invalid state type");
        }
        auto state = std::dynamic_pointer_cast<Array>(item);
        if (state->value.size() != 4 || state->value[0]->type != Object::Type::Integer || state->value[1]->type != Object::Type::Integer || state->value[2]->type != Object::Type::Boolean || state->value[3]->type != Object::Type::Array) {
            delete judger;
            throw VMError("(Wordle)Register_Judger", "Invalid state size");
        }
        int stateId = std::dynamic_pointer_cast<Integer>(state->value[0])->value;
        int color16 = std::dynamic_pointer_cast<Integer>(state->value[1])->value;
        bool mode = gVM->isTrue(state->value[2]);
        if (stateId < 0 || stateId > 255) {
            delete judger;
            throw VMError("(Wordle)Register_Judger", "Invalid state ID");
        }
        if (color16 < 0 || color16 > 15) {
            delete judger;
            throw VMError("(Wordle)Register_Judger", "Invalid color value");
        }
        std::set<int> overrides;
        auto& overridesArray = std::dynamic_pointer_cast<Array>(state->value[3])->value;
        for (const auto& override : overridesArray) {
            if (override->type != Object::Type::Integer) {
                delete judger;
                throw VMError("(Wordle)Register_Judger", "Invalid override type");
            }
            int val = std::dynamic_pointer_cast<Integer>(override)->value;
            if (val < -1 || val > 255) {
                delete judger;
                throw VMError("(Wordle)Register_Judger", "Invalid override value");
            }
            overrides.insert(val);
        }
        judger->ruleset.insert({stateId, {(char)color16, mode, overrides}});
    }
    options->judgers.insert({id, judger});
    return gVM->VNull;
}

std::shared_ptr<Object> Wordle_RegisterValidator(Args args) {
    if (!(options->pluginFeatures & PLUGIN_VALIDATORS)) {
        throw VMError("(Wordle)Register_Validator", "Plugin defined validators are not enabled");
    }
    if (args.size() != 2) {
        throw VMError("(Wordle)Register_Validator", "Invalid argument count");
    }
    if (args[0]->type != Object::Type::String || args[1]->type != Object::Type::Executable) {
        throw VMError("(Wordle)Register_Validator", "Invalid argument types");
    }
    auto id = std::dynamic_pointer_cast<String>(args[0])->value;
    if (options->validators.count(id)) {
        throw VMError("(Wordle)Register_Validator", "Id " + id + " cannot be redefined");
    }
    options->validators.insert({id, std::bind(ScriptValidator, std::placeholders::_1, std::dynamic_pointer_cast<Executable>(args[1]))});
    return gVM->VNull;
}

std::shared_ptr<Object> Wordle_RegisterProblemset(Args args) {
    if (!(options->pluginFeatures & PLUGIN_PROBLEMSETS)) {
        throw VMError("(Wordle)Register_Problemset", "Plugin defined problemsets are not enabled");
    }
    if (args.size() != 2) {
        throw VMError("(Wordle)Register_Problemset", "Invalid argument count");
    }
    if (args[0]->type != Object::Type::String || args[1]->type != Object::Type::Executable) {
        throw VMError("(Wordle)Register_Problemset", "Invalid argument types");
    }
    auto id = std::dynamic_pointer_cast<String>(args[0])->value;
    if (options->problemsets.count(id)) {
        throw VMError("(Wordle)Register_Problemset", "Id " + id + " cannot be redefined");
    }
    options->problemsets.insert({id, std::bind(ScriptProblemset, std::dynamic_pointer_cast<Executable>(args[1]))});
    return gVM->VNull;
}

std::shared_ptr<Object> Wordle_AddSearchEngine(Args args) {
    if (!(options->pluginFeatures & PLUGIN_SEARCH_ENGINES)) {
        throw VMError("(Wordle)Add_Search_Engine", "Plugin defined search engines are not enabled");
    }
    plain(args);
    if (args.size() != 2) {
        throw VMError("(Wordle)Add_Search_Engine", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::String || args[1]->type != Object::Type::String) {
        throw VMError("(Wordle)Add_Search_Engine", "Invalid argument types");
    }
    auto name = std::dynamic_pointer_cast<String>(args[0])->value;
    auto url = std::dynamic_pointer_cast<String>(args[1])->value;
    if (options->dictSearchEngines.count(name)) return gVM->False;
    options->dictSearchEngines.insert({name, url});
    return gVM->True;
}

std::shared_ptr<Object> Wordle_AddGamemode(Args args) {
    if (!(options->pluginFeatures & PLUGIN_GAMEMODES)) {
        throw VMError("(Wordle)Add_Gamemode", "Plugin defined gamemodes are not enabled");
    }
    plain(args);
    if (args.size() != 5) {
        throw VMError("(Wordle)Add_Gamemode", "Invalid number of arguments");
    }
    if (args[0]->type != Object::Type::String || args[1]->type != Object::Type::String || args[2]->type != Object::Type::String || args[3]->type != Object::Type::String || args[4]->type != Object::Type::String) {
        throw VMError("(Wordle)Add_Gamemode", "Invalid argument types");
    }
    auto id = std::dynamic_pointer_cast<String>(args[0])->value;
    auto name = std::dynamic_pointer_cast<String>(args[1])->value;
    auto judger = std::dynamic_pointer_cast<String>(args[2])->value;
    auto validator = std::dynamic_pointer_cast<String>(args[3])->value;
    auto problemset = std::dynamic_pointer_cast<String>(args[4])->value;
    if (options->gamemodes.count(id)) {
        throw VMError("(Wordle)Add_Gamemode", "Id " + id + " cannot be redefined");
    }
    if (!options->judgers.count(judger) || !options->validators.count(validator) || !options->problemsets.count(problemset)) {
        throw VMError("(Wordle)Add_Gamemode", "Invalid judger, validator or problemset");
    }
    JudgerType* v_judger = options->judgers[judger];
    ValidatorType v_validator = options->validators[validator];
    ProblemsetType v_problemset = options->problemsets[problemset];
    Gamemode gamemode = {v_judger, v_validator, v_problemset, name};
    options->gamemodes.insert({id, gamemode});
    return gVM->VNull;
}

void Plugins::Wordle::enable() {
    auto wordle = std::make_shared<NativeObject>();
    wordle->set("RED", std::make_shared<Integer>(4));
    wordle->set("GREEN", std::make_shared<Integer>(2));
    wordle->set("BLUE", std::make_shared<Integer>(1));
    wordle->set("LUMINA", std::make_shared<Integer>(8));
    wordle->set("BRIGHT", std::make_shared<Integer>(8));
    wordle->set("MARK", gVM->False);
    wordle->set("SPOILER", gVM->True);
    wordle->set("CORE", std::make_shared<String>("aho-corasick"));
    wordle->set("inDict", std::make_shared<NativeFunction>(Wordle_InDictionary));
    wordle->set("sizeDict", std::make_shared<NativeFunction>(Wordle_SizeDictionary));
    wordle->set("indexDict", std::make_shared<NativeFunction>(Wordle_IndexDictionary));
    wordle->set("createDict", std::make_shared<NativeFunction>(Wordle_CreateDictionary));
    wordle->set("addWord", std::make_shared<NativeFunction>(Wordle_AddWord));
    wordle->set("createJudger", std::make_shared<NativeFunction>(Wordle_CreateJudger));
    wordle->set("judgerConfig", std::make_shared<NativeFunction>(Wordle_JudgerConfig));
    wordle->set("judgerResult", std::make_shared<NativeFunction>(Wordle_JudgerResult));
    wordle->set("judgerSet", std::make_shared<NativeFunction>(Wordle_JudgerSet));
    wordle->set("registerJudger", std::make_shared<NativeFunction>(Wordle_RegisterJudger));
    wordle->set("registerValidator", std::make_shared<NativeFunction>(Wordle_RegisterValidator));
    wordle->set("registerProblemset", std::make_shared<NativeFunction>(Wordle_RegisterProblemset));
    wordle->set("addSearchEngine", std::make_shared<NativeFunction>(Wordle_AddSearchEngine));
    wordle->set("addGamemode", std::make_shared<NativeFunction>(Wordle_AddGamemode));
    regist("Wordle", wordle);
}
