# Mirekintoc Wordle 4.x

终端 Wordle，支持词库管理、本地账户、多语言、声明式插件及可选 Lua 规则插件。

## 构建

依赖：C++17、nlohmann-json、SQLite3、ICU、Lua 5.4/5.5；curses 后端另需 ncursesw 或宽字符 PDCursesMod。
支持系统包、pkg-config、CMake 包、自定义前缀和 vcpkg，不要求使用某个包管理器。

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cd build/bin
./wordle4
```

从生成了 `config.json` 的目录运行。CMake 会自动生成词库并复制资源。
依赖来源、后端选项及交叉编译见 `BUILDING.md`；运行配置见 `res/common/CONFIGURE.md`。

## 后端

`terminal.backend` 或优先级更高的 `WORDLE_BACKEND` 环境变量可选择：

| 值 | 实现 |
| --- | --- |
| `auto` | 交互式 POSIX TTY 优先用已编译的 `curses`，否则用 `ansi`；其余 Windows 控制台用 `win32`。 |
| `curses` | ncursesw 或 PDCurses 输入输出；链接的具体实现由构建配置决定。 |
| `ansi` | POSIX termios/poll 输入和 ANSI/VT 输出，支持 Unix、Cygwin 及提供这些 API 的环境。 |
| `win32` | 原生控制台输入记录和 Unicode 屏幕缓冲区，不输出 VT 序列。 |
| `win32-vt` | Win32 控制台输入；优先使用 VT 输出、VT 输入和 VT 鼠标，目标不支持时回退到原生输出。 |

界面需要 80×25 字符单元，支持缩放提示和退出恢复。ANSI/ncurses 要求交互式终端；
PDCurses 的能力取决于所选端口。不是所有 MinTTY/MSYS 会话都提供 Win32 控制台 API。
复杂/未知字素簇在布局前保守替换；Win32 采用单 BMP 字素单元，模糊宽度可配置。能力与限制见 `TERMINAL-TEXT.md`。
