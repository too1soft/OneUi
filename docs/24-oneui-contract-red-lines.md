# OneUI 集成契约与红线

日期：2026-05-26

## 你的要求，必须逐条执行

以下内容是下游产品接入 OneUI 的硬性要求，不是建议：

1. OneUI 必须提供所有基础组件能力和 CSS-like 样式能力。
2. 基础能力包括但不限于：布局、窗口、标题栏、按钮、输入框、只读输入框、禁用态、卡片、侧边栏、导航、图标、滚动、日志、弹窗、状态条、视频画面容器、远程输入区域。
3. Remote 写到任何 UI 功能时，必须先去 OneUI 查找是否已有对应能力。
4. 如果 OneUI 已有对应能力，Remote 必须直接使用 OneUI 的能力，不允许绕开。
5. 如果 OneUI 没有对应能力，必须先在 OneUI 中补齐通用、可复用、可文档化、可测试的能力，再回到 Remote 使用。
6. Remote 最终必须依赖 OneUI DLL，而不是在业务项目里复制一套临时 UI 实现。
7. OneUI 是后续 Remote、网云穿、iShell 以及其他产品共用的开源 UI 框架，因此新增能力不能写死 Remote 业务语义。
8. 任何“为了快，先在 Remote 里手写一个控件”的做法，默认违规。

## 红线

### 红线 1：Remote 不直接实现通用控件

Remote 禁止直接实现以下通用 UI 能力：

- Button。
- TextField / PasswordField / OtpCodeField。
- Checkbox / Radio / Switch。
- Select / ComboBox。
- Slider。
- Tabs / SegmentedControl。
- Card / Surface / Panel。
- Sidebar / NavItem。
- TopBar / SearchBox。
- CustomTitleBar。
- Dialog / Toast / Tooltip / ContextMenu。
- ScrollView / LogView。
- Icon / IconButton。
- StatusBar / Toolbar。
- Layout system。

如果 Remote 需要这些能力，必须来自 OneUI。

### 红线 2：Remote 不混用 Win32 子控件和父窗口自绘来实现同一个控件

禁止出现这种结构：

```text
父窗口自绘输入框外观
  + Win32 EDIT 子窗口负责真实输入
```

这种混合方案会导致：

- 点击编辑框闪烁。
- 输入区域和外框割裂。
- hover/focus/readonly/disabled 状态不统一。
- 光标、选择、IME、滚动文本难以控制。
- 业务侧反复修补，无法复用。

输入框必须是 OneUI TextField 这样的完整控件，由 OneUI 统一拥有绘制、状态和输入行为。

### 红线 3：Remote 不直接写产品级 CSS 解析器或绘制器

Remote 可以传入主题配置，但不能自己扩展 CSS 解析、样式合并、阴影、边框、focus ring、hover state。

这些能力必须在 OneUI StyleSheet / Theme / Renderer 中实现。

### 红线 4：OneUI 能力必须先验收，再下游集成

OneUI 每新增一个可复用能力，必须至少具备：

- 公共 API。
- 行为测试或截图测试。
- 文档。
- Gallery 或示例。
- Win32 后端实现。
- 明确 Win7 兼容性说明。

Remote 只能接入已经满足这些条件的能力。

### 红线 5：业务语义不得污染 OneUI API

OneUI API 禁止出现：

- device code。
- temporary code。
- session id。
- controller。
- host。
- Remote 专用文案。

这些属于 Remote 业务层。OneUI 只能提供通用组件，例如 `TextField`、`OtpCodeField`、`AppShell`、`RealtimeFrameView`。

### 红线 6：最终交付路径必须依赖 OneUI DLL

Remote 正式 GUI 的依赖关系应是：

```text
remote client
  -> oneui.dll
  -> remote core / bridge
```

不是：

```text
remote client
  -> 手写 Win32 UI
  -> 零散 GDI helper
  -> 临时 CSS parser
```

## 标准流程

Remote 任何 UI 需求都必须按以下流程执行：

```text
Remote 需要一个 UI 能力
  -> 查 OneUI 是否已有通用组件/API
    -> 已有：Remote 直接使用
    -> 没有：先在 OneUI 中设计并实现
      -> 写测试
      -> 写文档
      -> 写示例或 Gallery
      -> 验证截图和行为
    -> Remote 再集成 OneUI 能力
```

禁止跳过 OneUI 直接在 Remote 中实现。

## 方案是否合适

结论：合适，而且对当前产品矩阵是更优路线。

原因如下：

1. Remote、网云穿、iShell 都是桌面工具类产品，组件形态高度重叠。
2. 这些产品都需要轻量、原生、Win7/Win10 兼容、低内存、低包体积，不适合 WebView/Tauri/Electron。
3. Qt 授权模式不符合偏好，平台分别写 GUI 维护成本又太高。
4. 自研 OneUI 可以把长期成本从多个业务项目里收敛到一个 UI 基础设施项目。
5. OneUI 后续开源也能反向提升工程质量，因为 API、文档、示例、测试都必须通用化。

这条路线的价值不只是 Remote 的界面，而是形成一个可复用的轻量原生 UI 层。

## 方案是否可行

结论：可行，但必须接受短期慢、长期快。

可行的基础：

- OneUI 已经有 Win32 后端。
- OneUI 已经有 Skia raster 绘制路径。
- OneUI 已经有 `Widget` / `View` / `Canvas` 模型。
- OneUI 已经有 Button、TextField、Select、Tabs 等基础控件骨架。
- OneUI 已经有 StyleSheet 子集和主题 token。
- OneUI 已经有部分 Remote 相关原语方向：RealtimeFrameView、RemoteInputRegion、Monitor/DPI。

最大问题不是“能不能做”，而是“能不能严格不绕路”。只要 Remote 继续临时堆 Win32 控件，OneUI 就永远不会成熟；只要坚持 OneUI-first，问题会集中暴露在 UI 库里，并且每修一次所有产品都受益。

## 主要风险

### 风险 1：开发节奏短期变慢

每个控件都先补 OneUI，会比直接在 Remote 里写慢。

但这是必要成本。否则每个产品都会重复踩输入框、按钮、布局、弹窗、滚动、焦点、DPI、字体渲染的问题。

### 风险 2：OneUI 变成半成品 UI 框架

如果只补 API，不做测试、Gallery、截图验收，OneUI 会变成另一个不稳定抽象层。

应对方式：每个新增能力必须有行为测试或视觉截图测试。

### 风险 3：CSS-like 能力失控

如果目标变成完整浏览器 CSS，会膨胀。

应对方式：OneUI 只实现产品桌面 UI 所需的 CSS-like 子集：

- selector。
- pseudo state。
- spacing。
- layout。
- color。
- border。
- radius。
- shadow。
- focus ring。
- typography。
- state layer。

不追求浏览器完整特性。

### 风险 4：Win7 与现代渲染能力冲突

Win7 限制较多，尤其是 DirectComposition、现代透明窗口、部分字体/IME 行为。

应对方式：

- Win32 + Skia raster 作为保守默认后端。
- 高性能视频画面单独做 `RealtimeFrameView`，后续按平台优化。
- UI 控件优先稳定，不依赖 WebView。

### 风险 5：OneUI DLL 包体积

自研 UI 库如果依赖过重，会违背轻量目标。

应对方式：

- OneUI DLL 必须有 package size gate。
- 依赖必须有 runtime audit。
- Windows 产品构建优先 MSVC `/MT` 和可控静态依赖。
- MinGW 动态依赖只能用于开发调试，不作为默认分发。

## 近期执行顺序

### 第一阶段：OneUI TextField

这是当前最优先能力，因为 Remote 已经暴露了输入框问题。

必须完成：

- normal。
- hover。
- focused。
- readonly。
- disabled。
- placeholder。
- prefix/suffix。
- caret。
- selection。
- copy/cut/paste。
- select all。
- IME composition。
- 光标闪烁。
- 无闪烁点击与焦点切换。
- 状态矩阵截图。

Remote 暂停继续优化业务输入框。

### 第二阶段：OneUI CSS-like 能力

必须补齐：

- border 单边控制。
- outline / focus-ring。
- box-shadow。
- inset-shadow。
- state layer。
- typography。
- layout spacing。
- style parse error diagnostics。
- 外部 `.oui.css` 加载。

### 第三阶段：OneUI AppShell

必须补齐：

- CustomTitleBar。
- Sidebar。
- NavItem。
- TopBar。
- SearchBox。
- Card。
- IconButton。
- StatusBar。
- LogView。

### 第四阶段：Remote 重新接入

Remote 新建或切换到 OneUI 客户端，只做业务组合：

- 设备码区域。
- 临时验证码区域。
- 发起控制区域。
- 日志区域。
- 会话窗口。

Remote 不再拥有这些控件的底层绘制。

## 检查清单

任何 Remote UI PR 或开发任务开始前，必须回答：

1. 这个能力是不是通用 UI 能力？
2. OneUI 是否已有？
3. 如果已有，为什么不直接用？
4. 如果没有，OneUI 补了吗？
5. OneUI 有没有测试？
6. OneUI 有没有文档？
7. OneUI 有没有示例或截图？
8. Remote 是否只做业务组合？
9. 是否引入了新的 Win32 子控件混合绘制？
10. 是否仍满足最终依赖 OneUI DLL 的目标？

只要任何一项不满足，就不能继续在 Remote 里实现。

## 最终判断

该方案合适且可行。

它的短期代价是 Remote 界面会慢一点完成；长期收益是 OneUI 成为真正可复用、可开源、可承载多个产品的轻量原生 UI 基础设施。

从现在开始，Remote UI 的正确工作方式不是“业务里先写一个能看的”，而是“OneUI 先提供能力，Remote 再使用能力”。这条红线必须严格执行。
