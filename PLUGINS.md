# 插件文件格式

普通插件目录以 `manifest.json` 为入口；运行目录的必需核心插件使用 `core.json`。两者是相同的 `schemaVersion: 2` manifest 格式，贡献直接写在顶层，不使用 `resources` 或 `contributions` 中间层。未知字段报错。

```json
{
  "schemaVersion": 2,
  "id": "org.example.words",
  "name": "Example words",
  "version": "1.0.0",
  "author": "Example author",
  "license": "MIT",
  "dictionaries": {},
  "words": []
}
```

必需字段为 `schemaVersion`、`id`、非空 `name`、非空 `version`。可选元数据为 `author`、`homepage`、`description`、`license`；它们只接受普通字符串，不提供插件局部 i18n。资源 ID 只允许小写 ASCII 字母、数字、点、下划线和连字符，且必须有 `插件ID.` 前缀。

所有路径相对插件根目录。拒绝绝对路径、`..`、NUL、盘符、反斜线和逃出插件目录的符号链接。无目录扫描、通配符导入或隐式依赖。

Lua 插件可在 manifest 的 `files` 中声明文件别名和读取上限，再在 `permissions.read` 中授予脚本读取权限。`permissions.register` 按 ID 授权 Lua 主动注册 grader、dictionary 和 words；`permissions.log` 授权脚本写入宿主日志。内存、文件数量、单文件和总文件大小限制仍然有效。

## 核心插件

`core.json` 是固定先加载的标准插件，元数据为 ID `core`、author `Mirekintoc`、license `MIT`。它注册 `core.english` 并逐文件加载运行目录中的 `accept.dict` 与 `answer.dict`，同时声明四个核心 grader。核心 grader ID 为 `core.wordle`、`core.letter_presence`、`core.match_count`、`core.hardle`；它们的算法由程序内置的 C++ native runtime 提供，不属于普通插件可用的 runtime。`core.hardle` 按 Wordle 规则统计绿色和黄色数量，并将绿色状态、黄色状态从左到右集中显示，不暴露具体位置。

## 注册词典

`dictionaries` 只注册词典身份和字符规则，不携带词汇：

```json
"dictionaries": {
  "org.example.words.main": {
    "name": "Example",
    "equivalents": []
  }
}
```

字符域固定为小写 ASCII `a-z`，单词固定五字母。`equivalents` 是字符串数组；每组第一个字符是规范字符，例如 `["vu", "ij"]` 将 u 规范化为 v、j 规范化为 i。一个字符最多属于一组。

组中包含 `!` 时，该组其他字符全部禁用，例如 `"w!"` 禁用 w。禁用字符不能输入；导入时含禁用字符的词会被忽略；字母表仍显示该字符，但始终处于禁用状态。输入和答案在 grader 调用前规范化，历史回显保留用户原输入。

## 追加词汇

`words` 是扁平数组，每项只注册一个文件，并必须声明格式：

```json
"words": [
  {
    "filter": "org.example.words.main",
    "target": "accept",
    "file": "acceptable.txt",
    "type": "plain"
  },
  {
    "filter": "org.example.words.main",
    "target": "answer",
    "file": "answers.txt",
    "type": "plain"
  }
]
```

`filter` 为词典 ID 或 `*`；通配符也作用于之后加载的词典。`target` 为 `accept` 或 `answer`，答案会自动加入接受部分。`type` 必须是 `plain`、`json` 或 `nbt`。重复词自动去重。

`plain` 一行一个词，允许 LF/CRLF、空行和 `#` 整行注释。`json` 是含字符串数组 `words` 的对象；`nbt` 是含字符串列表 `words` 的 compound。

## Grader

游戏和词典查看器直接选择 grader，不再注册额外的模式资源。脚本插件在顶层 `graders` 中声明 grader；每个 grader 必须提供面向用户显示的 `name`，完整格式见 `PLUGIN-RUNTIME.md`。

每个 grader 最多有 256 个状态。状态颜色支持终端 16 色、RGB、CSS 命名色、独立的 `hidden`，其他颜色可附加样式；状态还声明 `mark` / `spoiler` 和覆盖关系。`deterministic: true` 的 grader 可用于词典筛选；非确定 grader 必须按 `PLUGIN-RUNTIME.md` 声明 `compatible` 导出。

## 预算与原子性

- 最多读取 128 个不同文件，合计 16 MiB，单文件最多 4 MiB；代码合计最多 1 MiB。
- 一个插件的词典、词汇和 grader 全部验证成功后才提交；错误会回滚当前插件。
- `core.json` 失败会终止启动；普通插件失败只记录诊断并继续加载其他插件。
- Lua 必须编译启用且在 `plugins.runtimes` 中允许。

附带插件：exdict 通过 `filter: "*"` 向所有词典追加词汇；Lingua Latina 注册拉丁语词典；Hardcore 提供独立 Lua grader。
