# Lua 规则后端

Lua 后端默认编译。当前能力是自定义 **grader（评测器）**。创建游戏时独立选择 grader、词典与答案来源，不再注册额外的模式资源。

## 场景与依赖

Lua 用于直接编写、修改小型本地规则，不需要编译工具链；依赖 Lua 5.4/5.5，仅接受文本 `.lua`，不接受字节码。

不要把 Lua 的受限环境当成对恶意代码的完整安全隔离。当前只建议启用可信来源的规则模块。不得在脚本中运行任意后台任务或持有 UI 对象。

## 启用与示例

先按 `BUILDING.md` 编译 Lua 后端，再在 `config.json` 中显式允许执行：

```json
"plugins": {
  "enabled": true,
  "runtimes": ["lua"],
  "load": [
    {"id": "org.wordle.lua", "path": "lua"}
  ]
}
```

将 `examples/plugins/lua` 复制到运行目录的 `plugins/` 内。示例只判断位置是否完全匹配，不是标准 Wordle 的黄色字母规则。
默认配置加载 Unix dict、Lingua Latina 与 Hardcore，并允许 Lua runtime；移除 Lua 或设置 `runtimes: []` 可禁用脚本执行。

## 清单扩展

```json
"runtime": [{"id": "lua", "backend": "lua", "entry": "code/main.lua", "modules": {"rules": "code/rules.lua"}, "exports": ["check"]}],
"graders": {
  "org.example.plugin.rule": {
    "name": "Example position rule",
    "runtime": "lua",
    "exports": {"check": "check"},
    "deterministic": true,
    "states": {
      "0": {"color": "slategray", "mode": "mark", "overrides": [-1]},
      "1": {"color": "gold", "mode": "mark", "overrides": [-1, 0]},
      "2": {"color": "term:RGL,bi", "mode": "spoiler", "overrides": [-1, 0, 1]}
    }
  }
}
```

资源 ID 必须有 `插件ID.` 前缀。`name` 是显示在游戏设置中的 grader 名称，必须是非空字符串。每个 grader 可声明最多 256 个状态，ID 为规范十进制 `0..255`。每个状态必须声明 `color`、`mode` 和 `overrides`：

- `color` 使用 `COLORS.md` 的完整表达式，支持终端 16 色、RGB、CSS 命名色和独立的 `hidden`；其他颜色可附加粗体、斜体等样式。
- `mode` 为 `mark` 或 `spoiler`；后者不把该颜色显示到字母表。
- `overrides` 列出该状态可以覆盖的旧状态；`-1` 表示尚无状态。引用的其他状态必须已声明。
- `deterministic: true` 表示固定输入必有固定输出。词典筛选还可声明 `compatible` 导出；它直接判断 `(guess, answer, feedback)` 是否相容，适用于带局内状态但可验证候选答案的规则。

这组能力覆盖旧 MPCC 的 256 状态、16 色、MARK/SPOILER 和覆盖关系，并扩展了颜色表达式。所有已声明导出都会在加载阶段验证；不支持某项能力的 backend 会拒绝该插件，而不会静默降级。
未知字段、路径逃逸、未允许或未编译的后端、无效颜色或引用全部导致当前插件加载失败，不留下部分注册结果。
`deterministic` grader 可用评分结果筛选词典；非确定 grader 只有提供 `compatible` 导出时才可筛选。筛选会逐词调用插件，应保持规则轻量。

## ABI v2

- Lua 模块执行后返回普通导出表。grader `(guess, answer)` 接收两个 ASCII 字符串并返回恰好五项的数组，每项是 `0..255` 的已声明状态 ID。
- Lua 可选导出 `compatible(guess, answer, feedback) -> boolean`、`start()` 与 `finish()`。`start` 在每局创建时由 runtime 重新播种 Lua 的通用随机状态后调用，`finish` 在该局释放时调用。
- 状态 ID 的视觉含义完全由 manifest 的 `states` 定义，不对 0/1/2 作特殊假设。
- 宿主先按所选词典规范化等价字符，再调用 grader。
- 每个插件独立运行时；该运行时内共享模块状态并串行调用。宿主不向模块传递 Options、Registry、指针或可变 UI 对象。

## 限制及失败行为

- 单份及全部代码源合计最多 1 MiB，其他文件预算见 `PLUGINS.md`。
- Lua 分配器上限 8 MiB，每次加载/调用约 100,000 VM 指令预算。
- Lua 仅开放受限 base/table/string/math；无 io/os/package/debug/coroutine、动态 load、pcall/xpcall 或宿主 IO。移除了模式匹配、重复字符串及转储等容易放大 C 层执行成本的入口。
- 预算不等于墙钟超时。Lua 的 C 标准库调用以及编译消耗不被这些 VM 指令/内存限制完整覆盖；后端仍在主线程执行小型、有限规则，不宣称能安全运行任意不可信代码。
- 运行时异常、预算耗尽或非法返回值会使当前运行时停止后续调用；当前游戏显示错误并回到主菜单，不退出整个程序。
- 插件实例由注册的函数共享持有，注册表销毁时释放；没有热卸载、动态库加载、文件/网络/进程权限或线程宿主 API。

## 多文件代码

`runtime.entry` 指定入口文件，路径始终相对插件根目录；旧 `runtime.module` 字段不保留。
Lua 可用 `runtime.modules` 将模块名映射到显式源码路径。入口示例 `return require("rules")`；模块本身返回表或其他值。
所有模块在受保护初始化中只编译为文本 chunk，首次 require 时执行并缓存结果；递归 require 检测循环。初始化/调用中的 require 共享同一指令和内存预算。
这里的 require 不是标准 package 搜索器：没有 package.path、package.cpath、系统 Lua 模块或文件扫描；未声明模块直接报错。模块源码不能逃出插件目录。

## 上游参考

- Lua 5.4 手册：https://www.lua.org/manual/5.4/manual.html

