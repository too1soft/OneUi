# OneUI 优先开发决策

日期：2026-05-26

> **历史决策记录**：决策本身仍有效（`24-oneui-contract-red-lines` 是其契约化延伸）。但下文"当前 OneUI 状态 / 缺口"清单是 2026-05-26 快照，其中多项已落地（如 TextField 只读/选择/caret/composition、StyleSheet 的 state layer / focus ring / read-only 等）——现状以源码为准。

## 决策

从现在开始，Remote 项目不再继续在业务客户端里手写复杂 UI 控件。Remote 只保留为远程核心能力的测试壳和 OneUI 集成验证项目。

所有通用界面能力先在 OneUI 中实现、测试和文档化，再被 Remote 使用。Remote 不再绕开 OneUI 去混合 Win32 子控件、父窗口自绘和临时样式补丁。

## 为什么必须切换

当前 Remote 原生界面的主要问题不是配色或间距，而是架构层面的混合绘制问题：

- 输入框外框由父窗口自绘，真实输入由 Win32 `EDIT` 子控件承担，焦点切换时需要移动、隐藏、显示子窗口，天然容易闪烁。
- 父窗口自绘和子控件绘制不在同一个渲染模型里，导致编辑框内部区域、只读态、hover、focus、disabled 无法统一。
- OneUI 样式能力被业务侧临时 CSS 和 GDI 绘制绕开，结果每修一个控件都会引入新的边界问题。
- Remote 的远控核心还需要大量验证，把精力消耗在临时 GUI 上会拖慢核心功能。

这说明继续修 Remote 里的临时界面收益很低。正确方向是把 OneUI 变成可承载真实产品的 UI 层。

## 当前 OneUI 状态

已经具备的基础：

- Win32 窗口后端。
- Skia raster 绘制路径。
- `Widget` / `View` / `Canvas` 基础模型。
- `Button`、`TextField`、`Select`、`Tabs` 等基础控件骨架。
- 控件状态：hover、pressed、disabled、focus-visible。
- 样式 token、局部 style override、基础 StyleSheet。
- Remote 需要的部分底座文档和原语：`RealtimeFrameView`、`RemoteInputRegion`、monitor/DPI 方向。

尚不能直接承载 Remote 主界面的关键缺口：

- TextField 还缺产品级 IME、中文输入法组合态、鼠标选择、双击选词、只读态、禁用态、光标闪烁、滚动文本、上下文菜单。
- StyleSheet 还不是完整 CSS 子集，至少缺 border sides、box sizing、state layer、focus ring、transition 规则和更严格的解析错误报告。
- Win32 后端需要稳定的 dirty rect / repaint 策略，鼠标移动不应引发整窗闪烁。
- 需要 OneUI 自己提供 AppShell、TitleBar、Sidebar、TopBar、Card、IconButton、SegmentedControl、StatusBar、LogView 等产品组件。
- 需要视觉回归截图测试，不能只靠人工肉眼一次次对比。

## 新标准

### 1. 控件必须由 OneUI 拥有

Remote 不直接创建 Win32 `EDIT`、`BUTTON`、`STATIC` 来拼复杂界面。复杂控件必须是 OneUI `Widget`。

允许 Remote 暂时保留极简 Win32 壳用于远控核心测试，但它不再作为最终 UI 迭代目标。

### 2. 样式必须由 OneUI 统一解析和绘制

OneUI 需要提供类似 HTML/CSS 的稳定样式子集，但不能只停留在字符串解析。每个能力必须有：

- public API 或 style 属性。
- 渲染实现。
- 状态覆盖规则。
- 行为测试或截图测试。
- 文档示例。

### 3. TextField 作为第一优先级

当前 Remote 暴露最多问题的是输入框，因此 OneUI 下一步应先把 TextField 打磨到产品可用：

- normal、hover、focused、readonly、disabled 状态。
- placeholder、prefix icon、suffix action、helper/error text。
- caret、selection、copy/cut/paste、select all。
- 中文 IME 组合输入。
- 光标闪烁和焦点可见策略。
- 只读态不可编辑但可选中复制。
- 禁用态不可聚焦，不显示输入光标。

### 4. 必须建立视觉验收

OneUI 需要有一个 `oneui_gallery` 或 `oneui_visual_tests`，固定渲染以下页面并输出 PNG：

- 深色远程协助主界面。
- 表单控件矩阵。
- TextField 状态矩阵。
- Button 状态矩阵。
- AppShell/Sidebar/TitleBar 状态。

每次修改必须先看截图。没有截图验证，不认为 UI 修改完成。

## Remote 后续使用方式

Remote 的 GUI 近期分两层：

- `remote-native-win`：继续作为远控核心测试壳，目标是能启动、连接、显示画面、调试日志，不追求精美。
- `remote-oneui-client`：等 OneUI 的 AppShell、TextField、Button、TitleBar、LogView、RealtimeFrameView 稳定后再接入，作为真正产品 UI。

在 OneUI 没准备好前，不再要求 `remote-native-win` 达到向日葵级别视觉。

## 分阶段计划

### P0：OneUI 输入和绘制稳定

- 完成 TextField 产品级状态和输入行为。
- 完成 readonly / disabled / focus-visible 的统一语义。
- 完成 Win32 后端 repaint 稳定性，鼠标移动不能整窗闪烁。
- 完成 StyleSheet 解析错误可诊断，非法属性不能静默导致整张样式失效。
- 增加 TextField 状态矩阵截图测试。

### P1：OneUI 产品壳组件

- AppShell。
- CustomTitleBar。
- Sidebar/NavItem。
- TopBar/SearchBox。
- Card/Surface。
- Icon/IconButton。
- SegmentedControl。
- StatusBar。
- LogView。

### P2：Remote 专用组件

- RealtimeFrameView 完整渲染。
- RemoteInputRegion Win32 原生输入接入。
- 会话窗口 Toolbar。
- 画质/分辨率/缩放控制。
- FPS/延迟/码率状态条。

## 验收口径

只有满足以下条件，OneUI 才可以重新承载 Remote 的正式界面：

- TextField 点击、输入、切换焦点无闪烁。
- readonly 和 disabled 状态视觉、行为都正确。
- 控件 hover、pressed、focused 有统一设计规范。
- 主窗口、标题栏、侧边栏、卡片、按钮、输入框能通过截图验收。
- Win7/Win10 上不依赖 WebView、Qt、Tauri 或外部运行时。
- Remote 不再直接混用父窗口自绘和 Win32 子控件来实现同一个控件。

## 结论

退一步只做 UI 库是正确选择。短期看会慢一点，但它能把问题从“每个业务界面都修一遍”变成“OneUI 控件一次性修好，所有产品复用”。这对 Remote、网云穿、iShell 和后续产品都更划算。
