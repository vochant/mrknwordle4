# 构建说明

## 基本构建

项目需要 C++17、CMake 3.16、nlohmann-json、SQLite3、ICU、Lua 5.4 或 5.5，以及一个终端后端（ncursesw 或宽字符 PDCurses）。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

可执行文件和运行资源会生成到 `build/bin`。从该目录运行 `./wordle4`。

日志最低等级通过 CMake 缓存变量 `WORDLE_LOG_LEVEL` 选择，可设为 `debug`、`info`、`warn` 或 `error`，默认是 `info`：

```sh
cmake -S . -B build -DWORDLE_LOG_LEVEL=debug
```

## 依赖查找

构建优先使用 CMake 包配置，其次使用 `pkg-config`，最后使用系统头文件或库。可通过 `CMAKE_PREFIX_PATH` 提供自定义依赖前缀；vcpkg 环境使用其 toolchain 文件即可。

## 交叉编译

交叉编译时，词典合并工具需要可在构建机器执行。通过 `WORDLE_MERGE_EXECUTABLE` 提供原生 `merge` 工具，或配置 `CMAKE_CROSSCOMPILING_EMULATOR`。i18n 的 key 检查工具同样可通过 `WORDLE_CHECK_I18N_EXECUTABLE` 指定原生程序。

## 资源

CMake 会生成接受词和答案词典，并复制 `res/common` 与 i18n 资源。`check_i18n` 只比较每个语言目录的 message key 是否与基础语言一致，随后复制通过的目录；不做模板或参数的额外检查。
