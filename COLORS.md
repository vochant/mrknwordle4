# 颜色表达式

配置高亮、排除词颜色、插件规则颜色和控件颜色统一使用 `Color`（颜色值 + 样式位），游戏历史及词典显示不再用单字节存储颜色。

| 表达式 | 含义 |
| --- | --- |
| `term:R?G?B?L?` | 传统 16 色，R/G/B/L 各可选且必须按该顺序；`term:` 为黑，`term:RGBL` 为亮白 |
| `rgb:abc` | 24 位 RGB 简写，等于 `rgb:aabbcc` |
| `rgb:AaBBcc` | 六位十六进制，不区分大小写 |
| `red`、`rebeccapurple` | CSS Named Colors 名称，ASCII 大小写不敏感 |
| `term:RGL,bi` | 16 色与 bold + italic 样式组合 |
| `rgb:fff,bi` | RGB 颜色与多个样式组合 |
| `hidden` | 独立的隐藏值：将文字或符号替换为相同显示宽度的空格 |

颜色表达式最多包含一个逗号。逗号后的单个样式串可组合：`b` bold、`i` italic、`u` underline、`d` dim、`k` blink、`r` reverse、`s` strikethrough。空样式串合法，因此 `term:RGL,` 等同于 `term:RGL`；不允许 `term:RGL,b,i` 这样的多个逗号。
不接受空表达式、未知或重复样式、无效十六进制、旧的裸 `RGL` 或乱序/重复的 term 分量。不自动忽略空格。

名称表采用 W3C CSS Color 的完整 **148 个 Named Colors**（包含 gray/grey 等别名），固化于 `src/css_colors.inc`，运行时不联网。
在线标准列表：[W3C CSS Color — Named Colors](https://www.w3.org/TR/css-color-4/#named-colors)。
`transparent`、`currentColor` 和系统色不是这里的 Named Colors；字符终端不实现 CSS alpha 合成或继承语义，因此拒绝这些值。

## 后端降级

- Unix TTY：输出 24 位 RGB / 16 色和 SGR 样式；具体显示取决于终端能力。
- ncurses：只依 terminfo 宣告的能力工作，不根据 `COLORTERM` 推测，也不会改写 `TERM`。direct color 时，`cursesAnsi16: "auto"` 视 ANSI 16 为不兼容；可显式设为 `"on"` 尝试保留。非 direct color 时按 `COLORS` 回退：恰为 256 使用 xterm-256 色，恰为 88 使用 xterm-88 色，16..255 使用 ANSI 16，8..15 使用 ANSI 8（亮色降为普通色）；低于 8 或没有颜色能力会拒绝初始化。
- PDCursesMod：使用其稳定的 direct color API，不采用 ncurses 的 terminfo 颜色编码；粗体、暗淡、下划线、闪烁、反显以及可用时的斜体通过 curses 属性呈现。不支持的样式忽略。
- 原生 Win32：优先启用虚拟终端输出，从而使用 24 位 RGB、16 色和全部 SGR 样式；无法启用时映射为最接近的 16 色，仅保留下划线和反显，bold 不再改变颜色。

`hidden` 不是 CSS 颜色、包装函数或 SGR conceal 样式，也不能带逗号或样式。`hidden(red)`、`red,h`、`hidden,b`、`hidden,` 均拒绝。
当前景或背景为 `hidden` 时，文字和边框符号会替换为相同显示宽度的空格；双宽字符对应两个独立空格。该绘制状态的全部样式都会清空，布局宽度不变。

内部富文本由 `colorEscape()` 生成独立的颜色/背景标记，格式是 `\x02前景表达式;背景表达式\x03`；颜色和样式都参与脏单元比较。旧单字节色码解析已移除，不迁移旧历史中的颜色标记。
