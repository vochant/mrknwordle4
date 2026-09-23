# 配置

运行目录必须同时包含 `config.json` 和 `core.json`，两者使用 `schemaVersion: 2`。未知字段会被拒绝。

- `config.json` 只描述程序行为、默认选择和插件加载列表。
- `core.json` 是 ID 为 `core` 的必需标准插件，提供 `core.english` 词典；它固定先于可选插件加载，加载失败时程序终止。
- 核心词表是运行目录中的静态 `accept.dict` 与 `answer.dict`，无需下载或额外生成。

## 文本高亮

`config.json` 的 `highlight` 提供完整的终端 UI 色板，所有值均使用 `COLORS.md` 的颜色表达式：

```json
"highlight": {
  "foreground": "term:RGL",
  "background": "term:",
  "muted": "term:L",
  "selected": "term:RGL",
  "hover": "term:RGL",
  "inputActive": "term:GL",
  "inputPlaceholder": "term:L",
  "error": "term:RL",
  "dictionaryImpossible": "term:L",
  "accept": "term:RGB",
  "answer": "term:GL,b"
}
```

`foreground`、`background` 是普通 UI 的前景和底色；`muted` 用于禁用项及辅助内容；`selected` 与 `hover` 分别用于键盘选中和鼠标悬浮；`inputActive`、`inputPlaceholder` 控制输入框；`error` 用于输入校验错误。`dictionaryImpossible` 用于词典查看器中显示不可行词时的文字颜色。词典查看器对答案使用 `answer`，对其余接受词使用 `accept`；不再提供 scope、优先级或可选规则列表。

## 创建页默认值

- `defaults.dictionary`：游戏与词典查看器的默认词典，默认 `core.english`。
- `defaults.grader`：游戏与词典查看器的默认 grader，默认 `core.wordle`。
- `game.answerOnly`：是否只从 `answers` 中抽取答案；为 `false` 时从全部 `acceptable` 词中抽取。
- `game.validation`：是否要求猜词属于当前词典的 `acceptable` 集合。
- `game.maxGuesses`：-1/0 表示不限，正数为上限。
- `game.showAlphabet`：是否显示字母表。
- `dictionary.answerOnly`：查看器是否只显示 `answers`；为 `false` 时显示全部 `acceptable` 词。该项不改变词汇有效性检查。
- `dictionary.validation`：输入筛选条件时，是否要求它属于当前词典的 `acceptable` 集合。
- `dictionary.cleanupOnExit`、`showIds` 和 `showImpossible` 控制查看器行为；不可行词颜色由 `highlight.dictionaryImpossible` 控制。

游戏直接选择 grader，不再经过额外的模式资源。内置 grader 为 `core.wordle`、`core.letter_presence`、`core.match_count`、`core.hardle`，插件 grader 与它们处于同一级。

## 终端、语言与存储

- `terminal.backend`：`auto` / `ansi` / `curses` / `win32` / `win32-vt`。`win32` 始终使用原生 Console API；`win32-vt` 明确要求时才优先启用 VT 输出、输入和鼠标，无法启用则回退到原生路径。
- `terminal.mouse` 和 `terminal.handleInterrupts` 控制输入行为。
- `terminal.cursesTerm`：供 Curses 调用 `newterm` 的 terminfo 名；空字符串（默认）表示不替换当前终端。此项只提供显式尝试某个 terminfo 的能力，目标不存在或实际终端不兼容时初始化会失败。
- `terminal.cursesAnsi16`：枚举字符串 `"auto"`（默认）、`"on"` 或 `"off"`；旧布尔值和 `"true"` / `"false"` 仍兼容。direct color 时，`auto` 视 ANSI 16 为不兼容，`"on"` 才尽力保留 ANSI 0..15；非 direct color 时此项不影响回退。`terminal.cursesReservedColors` 用于 direct color 的 RGB 编号避让，范围 0..256，默认 256，即保留 0..255；设置此项不改变 terminfo 本身的颜色模型。
- `locale.language` 使用 BCP 47；`calendar` 默认 `default`，`timeZone` 默认 `local`。
- `storage.wal`：枚举字符串 `"off"`、`"checkpoint"` 或 `"keep"`；默认配置使用 `"checkpoint"`，旧数值 0/1/2 仍兼容。

## 搜索与插件

- `dictionary.search.engines` 是搜索引擎 ID 到 HTTP(S) URL 的映射，每个 URL 必须恰含一个 `%s`。
- `plugins.load` 是 `{id,path}` 数组，目录相对运行目录的 `plugins/`，入口固定为 `manifest.json`。
- `plugins.runtimes` 可允许 `lua`，且该后端必须在构建时启用。
- `plugins.enabled: false` 只关闭可选插件，不关闭必需的 `core.json`。

默认可选插件为 Unix dict、Lingua Latina 与 Hardcore；默认配置因此允许 Lua runtime。移除 Hardcore 或将 `plugins.runtimes` 设为空可禁用脚本执行。
