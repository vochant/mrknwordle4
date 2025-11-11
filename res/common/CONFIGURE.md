# Mirekintoc Wordle 4.x 配置指南

1. `core : string` 用于识别配置文件版本，以一种算法命名，本指南仅针对 `aho-corasick` 版本，请勿更改。
2. `base : object` Wordle 基本配置。
   - `answers/accepts : array` 声明用于存储答案词库/合法词库的文件，每个文件为一个 `object`，其中包括 `file` 字段（用于存储的文件名）和 `type` 字段（存储格式，可以是 `plain`、`json`、`nbt` 之一）。
   - `validation : boolean` 是否启用校验，关闭将不检查输入是否为合法词库内的单词。
   - `limit : integer` 猜测次数上限，`-1` 表示不设置上限。
   - `virtualTerminal : boolean` 呈现复杂内容的方式，开启表示尽可能使用控制台虚拟终端序列，关闭表示尽可能使用 Win32 API。
   - `mouseControlling : boolean` 鼠标控制模式，开启将不兼容 `Windows Terminal`、`MinTTY` 等非传统控制台。
   - `charmap : boolean` 在游戏中显示字符表。
   - `codepage : integer` 运行代码页，`-1` 表示更改代码页。除非您很清楚自己在做什么，否则不建议更改此项。不匹配程序编码或本地化文件的代码页或导致乱码。
   - `handleInterrupt : boolean` 捕获非正常终止程序，包括 `Ctrl+C/Ctrl+Break` 组合键、 关闭窗口、注销，不包括应用程序异常。除非您很清楚自己在做什么，否则不建议更改此项。少数情况下关闭该选项将导致数据损坏或丢失。
   - `wal : integer` Write-Ahead Logging 配置，`0` 表示禁用，`1` 表示启用且每次退出程序时清空，`2` 表示启用且完全由 SQLite 系统机制管理。其他值等同于 `0`。
   - `language : string` 显示语言，您应当保证对应的 `.lang`（JSON 格式的本地化文件）存在。如果您不会配置 `codepage`，请不要设置为非以 `.UTF-8` 结尾的值。
   - `calendar : string` 历法，接受的值包括 `default`（不指名）、`gregorian`、`buddhist`、`chinese`、`japanese`、`islamic`、`islamic-civil`、`islamic-umalqura`、`roc` 等
3. `dictionary : object` 词典管理设置。
   - `validation : boolean` 在词典中启用规则合法性校验。该项的作用不同于 `base.validation`。
   - `cleanup : boolean` 退出词典时清空临时数据，包括载入的主词库和规则集。
   - `highlights : array` 词汇高亮设置。每一项规则均为一个 `object`，其中 `category` 表示来源词库（`*` 代表其余全部），`color` 表示颜色（16 色，`R`=红，`G`=绿，`B`=蓝，`L`=高亮，其他字符无效，重复无效）。
   - `pageSize : boolean` 每页显示词汇数量。
   - `showId : boolean` 显示在所选词库中的编号。
   - `showImpossible : boolean` 显示根据规则已经排除的词汇。
   - `impossibleColor : string` 显示已经被排除的词汇使用的颜色。
   - `searchEngines : array` 用于搜索词汇的搜索引擎，每一项均为一个 `object`，其中 `id` 表示名称，`url` 表示网址（其中 `%s` 表示被搜索的词汇且只能出现一次）。
   - `search : boolean` 启用搜索。
   - `searchEngine : string` 默认搜索引擎，必须为 `searchEngines` 中某项的 `id`。
4. `plugins : object` 插件管理。
   - `enabled : boolean` 启用插件系统。
   - `scripting : boolean` 允许插件使用脚本（`.mpc` 文件）。
   - `dynamicLibraries : boolean` 允许插件脚本使用动态链接库。除非您很信任插件的作者，或对插件文件进行了充分的检查，否则您不应该开启此项，这将让您的电脑暴露于可能存在的危险之中。
   - `system : boolean` 允许插件脚本使用系统命令处理。除非您很信任插件的作者，或对插件文件进行了充分的检查，否则您不应该开启此项，这将让您的电脑暴露于可能存在的危险之中。
   - `prependLocation : boolean` 在插件配置文件中的所有路径前添加插件路径。这是大多数插件所需要的。该配置对脚本的文件操作无效。
   - `isolated : boolean` 隔离各插件脚本的跟级作用域。这将阻止插件从其他插件获取或更改信息，但会使得可能存在的依赖关系失效。
   - `enableFeatures : array` 启用的功能。每一项均为字符串，有效的值包含 `dictionaries`、`words`、`searchEngines`、`judgers`、`validators`、`problemsets`、`languages`，其中 `dictionaries` 隐含 `words`。
   - `manifest : string` 插件清单文件名。如果没有特殊的需求，不建议改为 `manifest.json` 以外的值。
   - `loadeds : array` 已加载的插件。每一项均为一个 `object`，其中 `directory` 表示存放插件的目录（`plugins` 的子目录），`id` 表示插件内部名称。您应当保证 `id` 与清单文件中的 `id` 相同，否则将不会加载插件。