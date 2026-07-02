# OneUI Platform Backend Contract

日期：2026-05-28

本文定义 OneUI 的跨平台后端契约。目标是让 `core`、`controls`、`layout`、`style`、`animation` 等组件层保持平台无关；Windows、macOS、Linux 只实现自己的 platform backend。下游产品，例如 Remote、网云穿、iShell，只能通过 OneUI C ABI 和通用组件组合界面，不能绕过 OneUI 直接写平台 GUI。

## 当前状态

- 已实现：Win32 backend。
- 未实现：macOS Cocoa backend、Linux X11/Wayland backend。
- 已具备跨平台结构：组件层、布局层、样式层、C ABI 与 `Window` 抽象已经分离。
- 尚未完成跨平台运行：macOS/Linux 目前仍是 skeleton，必须补齐窗口、事件、输入、渲染、DPI、剪贴板等后端能力。

这意味着以后支持 macOS/Linux 时，不应该重写 `Button`、`TextField`、`ProductShell`、`NavItem`、`Tile` 等组件；应该补齐平台后端，让这些组件继续复用。

## 分层红线

1. `include/oneui/controls` 和 `src/core` 不得包含 Win32、Cocoa、X11、Wayland 头文件。
2. `src/capi` 不得调用 Win32/Cocoa/X11/Wayland API；C ABI 只能调用 OneUI 抽象。
3. 平台后端实现只允许放在 `src/platform/<platform>`。
4. 组件层只能使用 OneUI 抽象：`Window`、`Canvas`、`Clipboard`、`Widget`、`CursorKind`、`MouseEvent`、`KeyEvent`、`TextInput`、`StyleSheet`。
5. 下游产品不得直接依赖平台窗口句柄实现通用 GUI 行为。

## 后端必须实现的能力

### 1. Window Lifecycle

每个平台必须实现 `oneui::Window`：

- `Window::create(WindowOptions)`
- `show`
- `run`
- `close`
- `minimize`
- `toggleMaximize`
- `setFullscreen`
- `setBorderless`
- `setTitle`
- `clientSize`
- `nativeHandle`

要求：

- `WindowOptions` 是跨平台输入契约，不能让调用方传平台专属结构。
- `nativeHandle` 只用于高级集成和诊断，普通控件不得依赖它。
- 最大化、还原、全屏、退出全屏必须尽量使用平台原子操作，避免白屏、旧帧闪烁和可见中间态。

### 2. Event Loop And Scheduling

后端必须提供：

- UI 线程事件循环。
- `post(std::function<void()>)` 跨线程投递。
- `requestAnimationFrame(std::function<void(double)>)`。
- 高精度、低抖动的 timer。
- 鼠标、键盘、文本输入、焦点、窗口尺寸变化事件分发。

要求：

- 高频输入事件不得同步执行昂贵重绘。
- hover/focus/pressed/caret 只能标记 dirty rect 并交给统一调度。
- animation frame 只在有活动动画时继续调度。

### 3. Rendering And Presentation

后端必须实现 Skia 呈现路径：

- 平台 surface 创建与复用。
- dirty rect clip。
- 局部 repaint。
- resize 时 surface 生命周期管理。
- blit/present 到窗口。
- 性能 trace 输出。

要求：

- 不擦背景。
- 不依赖全窗口每帧重绘。
- resize 时不能为每个尺寸消息强制同步满屏 paint。
- 阴影、渐变、文本 blob、文本测量必须使用 OneUI 通用 primitive/cache。

### 4. Input

后端必须把平台输入事件转换为 OneUI 通用事件：

- mouse move/down/up/wheel。
- keyboard down/up。
- text input。
- focus in/out。
- hover enter/leave。
- cursor query/update。

要求：

- 文本输入必须走平台 IME/text input 语义，不能只靠 keydown 猜字符。
- `TextField` 上 hover 必须显示 text cursor。
- cursor 更新必须跟随 hit-test 结果，不得由下游产品手动控制。

### 5. IME And Text Editing

后端必须支持：

- IME composition 开始、更新、提交、取消。
- composition caret rect。
- selection/caret repaint。
- Unicode 文本输入。
- 复制、剪切、粘贴。

短期可以先实现基础 text input，但必须保留 IME 扩展点，不允许把英文键盘路径写死为最终方案。

### 6. Clipboard

每个平台必须实现 `SystemClipboard`：

- `setText(std::wstring)`
- `text() const`

要求：

- OneUI 控件只依赖 `Clipboard` 抽象。
- 下游产品不得绕过 TextField 或 Clipboard 抽象直接读写平台剪贴板。

### 7. DPI, Monitor, And Geometry

后端必须实现：

- 当前窗口 DPI。
- 多显示器枚举。
- 工作区与屏幕区域。
- 坐标转换。
- DPI 改变事件。

要求：

- 组件层内部使用 OneUI `Point`、`Size`、`Rect`。
- 平台像素和逻辑像素转换必须在 backend 边界完成。

### 8. Window Chrome

后端必须支持 OneUI 自绘标题栏：

- borderless window。
- hit-test resize border。
- drag region。
- minimize/maximize/close。
- maximized/fullscreen state sync。

要求：

- 标题栏按钮视觉由 OneUI 组件绘制。
- 平台后端只负责窗口行为与命中测试。
- Win7 fallback 必须保留，不得引入 WebView 或 DirectComposition-only 必需路径。

### 9. Cursor

后端必须把 OneUI `CursorKind` 映射到平台 cursor：

- default
- pointer
- text
- resize horizontal/vertical/diagonal
- move
- not allowed

要求：

- cursor 映射是平台后端职责，不是 Remote 职责。
- hover 切换 cursor 不得触发整窗重绘。

### 10. Accessibility

后端最终应提供平台可访问性桥接：

- role
- name
- description
- value
- focus
- action

短期可以先保留 OneUI 内部 accessibility metadata，但平台后端必须保留对接空间。

## 平台后端验收清单

一个新平台 backend 合入前必须通过：

1. 创建窗口、显示、关闭、最小化、最大化、还原、全屏、退出全屏。
2. 鼠标 hover 跟手，无明显延迟。
3. 鼠标点击、键盘输入、文本输入、复制粘贴可用。
4. TextField caret 闪烁、位置、selection、text cursor 正常。
5. resize 连续拖动不闪烁、不白屏、不严重掉帧。
6. 样式、阴影、渐变、圆角、文字、图标渲染一致。
7. `ONEUI_RENDER_TRACE=1` 能输出关键指标。
8. Remote shell 页面无需业务代码变更即可运行在该平台。

## 已落地自动化

当前已新增 `oneui_backend_contract_tests`，用于验证平台后端基础契约：

- hidden window 创建与 client size。
- `setContent`、`setTitle`、`setBorderless`、`setFullscreen`、`toggleMaximize`、`minimize`、`requestRedraw`。
- `post` 能进入 UI 事件循环并关闭窗口。
- `requestAnimationFrame` 能回调并提供正向时间戳。
- `SystemClipboard` 文本 round-trip，并在结束时恢复原文本。

该测试必须保持只依赖 OneUI 抽象，不得包含 Win32/Cocoa/X11/Wayland API。后续 macOS/Linux backend 接入后，也必须让同一组测试通过。

## 当前审计结果

本轮审计发现 C ABI 曾直接包含 `windows.h` 并调用 `ShowWindow` 实现最小化。该逻辑已经收回到 `Window::minimize()`，C ABI 现在只调用 OneUI 抽象。

这是后续所有平台适配的标准：如果 C ABI 或组件层需要某个窗口行为，先补 OneUI 抽象，再由各平台 backend 实现，不能在 C ABI 或下游产品里写平台分支。

## 后续路线

1. 继续保持 Win32 backend 稳定。
2. 补充 backend smoke：窗口状态、输入、resize、clipboard、cursor。
3. macOS 优先实现 Cocoa window skeleton：窗口生命周期、Skia surface、事件循环、鼠标/键盘、clipboard。
4. Linux 再按实际发行目标选择 X11 first 或 Wayland first。
5. 每补一个平台能力，都先落入 OneUI 抽象和测试，再给 Remote 使用。
