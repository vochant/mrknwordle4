# Mirekintoc Wordle 4.x

终端 Wordle，支持词库管理、本地账户、多语言、声明式插件及可选 Lua 规则插件。

## 构建

依赖：C++17、nlohmann-json、SQLite3、ICU、Lua 5.4/5.5；curses 后端另需 ncursesw 或宽字符 PDCurses。
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

## 代码边界

- `src/main.cpp`：按序加载配置、语言、资源注册表、插件、数据库和终端会话。
- `src/options.cpp`：纯配置解析与校验，不加载词库或插件。
- `src/registry.cpp`：词库、内建规则和游戏模式；`src/plugins/manager.cpp`：声明式资源加载。
- `src/ui/`：界面组装及各业务页面；`include/ui/screens.hpp` 只声明页面工厂。
- `src/widgets/`、`src/paintbrush/`：控件与字符单元绘制，不直接访问平台 API。
- `src/terminal/`：会话生命周期、差异缓冲和终端后端。
- `src/eventbus.cpp`：线程安全的事件队列；固定具名事件定义在 `include/eventbus.hpp`。
- `CMakeLists.txt`：构建目标和源码列表；`cmake/` 负责依赖和平台发现。

输入、事件分发、控件修改和渲染都在主 UI 线程执行。渲染限制约 30 FPS，未变化的内容不提交。
后台线程只能通过 `evbus->send(...)` 传回事件，不能直接操作控件；销毁总线前必须停止并 join 生产者。
回调在队列锁外执行，订阅变更不影响当前事件的监听器快照。耗时业务回调仍可能暂时阻塞界面。

```cpp
evbus->send(Events::Route{"main"});
evbus->listen<Events::Route>([](const Events::Route& event) {
    navigate(event.name);
});
```

Unix 剪贴板需要 `wl-copy` / `xclip`，macOS 使用 `pbcopy`；外部浏览器使用 `xdg-open` / `open` / Windows ShellExecute。
配置与插件清单只接受 schemaVersion 2；旧格式、MPCC、动态库和旧脚本宿主接口已删除。
声明式插件见 `PLUGINS.md`，Lua 使用方式见 `PLUGIN-RUNTIME.md`。

颜色表达式与 W3C 名称表见 `COLORS.md`。国际化采用类型化消息、独立完整的语言目录及 UTF-8 资源，见 `I18N.md`。使用 `locale.language: "la"` 可启用拉丁语；日期数据取自固定 CLDR 快照，基数按本项目指定的 one/other 规则。

附带独立词典：`exdict` 的 `unix`（68 个重写后的 Unix/C 词）和 `Lingua Latina`（22,330 个接受词、6,951 个答案）；后者的数据来源和发布许可注意见插件 `SOURCES.md`。词典拥有自己的接受/答案部分、字符集和等价字符，不再绑定游戏模式。创建游戏或查看器时在单页调整多项配置，确认后生效；字体样式须显式声明词典作用域，见 `res/common/CONFIGURE.md` 与 `PLUGINS.md`。
