# OneUI 架构

本文描述当前 `0.1.0` 工作区的真实架构。公开签名仍以 `include/oneui`、
`oneui_c_api.h` 和 Rust crate 为准。

## 设计目标

OneUI 是一个原生、自绘、保留式桌面 UI 框架，当前专注 Win32。核心目标是：

- 在没有浏览器/WebView 的前提下提供一致的桌面控件与产品外壳；
- 把平台窗口、输入、DPI、剪贴板、文件对话框与跨平台控件逻辑分离；
- 让 C++、C ABI 和 Rust 产品看到同一套行为，而不是三套独立组件；
- 通过行为测试、后端契约和布局树快照验证真实交互与几何；
- 避免框架层出现具体产品名、页面分支和业务 magic number。

## 分层

```text
Product application
  C++ API | UTF-8 C ABI v16 | safe Rust API
                         |
Interop and ownership boundary
  opaque handles | copied UTF-8 data | callbacks | dispatcher
                         |
Retained UI core
  Widget/View tree | state/binding | selection | layout | focus
  input routing | overlays | animations | accessibility metadata
                         |
Style and rendering
  StyleSheet | typed adapters | Canvas | Skia raster
                         |
Platform backend
  Win32 window/messages/DPI/IME path/clipboard/dialogs/tray/presentation
```

### 1. 公开 C++ 层

`include/oneui/` 包含公开 API：

- `widget.h` / `view.h`：控件基类、树、事件、焦点、tooltip 与语义；
- `controls/`：输入、数据、反馈和专用视图；
- `layout/`：容器、浮层、分栏和产品壳；
- `style*.h`：样式盒、CSS-like 解析、typed adapter 和过渡；
- `platform/`：窗口、显示器与平台服务抽象；
- `oneui_c_api.h`：跨语言稳定形状。

公开 C++ 类型不跨 C ABI 边界暴露 STL、异常或 RTTI。

### 2. Core

`src/core/` 实现平台无关的保留式控件树：

- `Widget` 保存 frame、preferred size、可见/禁用/焦点状态、样式节点、tooltip 和可访问性元数据；
- `View` 管理子树、深层命中、焦点链、tooltip 查询和绘制顺序；
- 布局容器在 commit/layout 阶段为子项分配逻辑像素 frame；
- 控件在 `paint(Canvas&)` 中自绘，不创建原生 Win32 child control；
- 输入从窗口根按命中/焦点路由到深层控件；
- `State<T>` / `Binding<T>` 提供轻量响应式状态；
- `SelectionModel` 统一 List、VirtualList、Tree/Table 等的 Ctrl/Shift 选择语义。

### 3. Canvas 与 Skia

`Canvas` 是绘制抽象，支持：

- 矩形、圆角、椭圆、线、图标 primitive；
- 文本、命名字体、字重和对齐；
- 线性/径向渐变、阴影、clip、save/restore；
- BGRA/RGBA 像素帧绘制；
- dirty `clipBounds()` 与不随局部裁剪变化的 `viewportBounds()`。

Win32 后端当前使用 Skia raster。Core 不直接包含 Win32 HWND/GDI 消息逻辑。

### 4. 样式系统

`StyleSheet` 解析受控 CSS-like 子集：

- tag、class 与 `:hover` / `:active` / `:focus` / `:disabled` / `:selected` / `:read-only`；
- custom property 与 `var(--token)`；
- background、foreground、border、radius、padding、gap、尺寸、字体、scrollbar、shadow 和 transition；
- selector specificity、规则顺序、resolve cache 和运行时 token 刷新；
- typed adapter 把统一 `StyleBox` 映射到 Button、TextField、Tabs、Table 等控件状态。

它不是浏览器 CSS。布局只支持 OneUI 明确定义的属性和容器语义。

### 5. Overlay 与高层绘制

`OverlayHost` 管理普通内容和按 layer 排序的 overlay：

- modeless、light-dismiss、modal；
- anchored overlay、焦点 trapping、外部 pointer 阻断；
- 嵌套 View / ScrollView / OverlayHost 的越界绘制和命中；
- popup 关闭后的焦点恢复。

`Widget::paintsAboveSiblings()` 与深层 `hitTest()` 使 Select/Popup 能在父容器边界外仍被根节点正确路由。

### 6. 平台层

当前唯一可运行后端位于 `src/platform/win32/`，负责：

- 窗口创建、初始化、显示、消息循环、激活、最小化/最大化/全屏；
- borderless 标题栏命中、可拖拽区与交互附件区；
- logical client size、physical pixel size 与 DPI scale；
- 鼠标、滚轮、键盘、raw key、文本输入和 IME 路径；
- Skia surface/presentation 与重绘调度；
- 显示器、窗口 placement、剪贴板、文件/目录选择、confirm/prompt 和托盘；
- tooltip 窗口与布局树同步快照。

Linux/macOS 文件是未接线骨架；调用窗口/剪贴板会明确失败，避免静默退化。

## 互操作层

### C ABI

`src/capi/oneui_c_api.cpp` 把 C++ 类型包装成 opaque handles。边界规则：

- 新字符串使用带长度的 UTF-8 view；
- 结构化数组在函数返回前被拷贝；
- 句柄由显式 destroy 释放调用方引用；
- 控件树/overlay 在挂载后持有共享生命周期，销毁外部 wrapper 不会提前删除已挂载节点；
- callback 由函数指针 + `user_data` 表示；清除 callback 后不再触碰调用方数据；
- ABI 版本必须在加载时检查，目前为 v16。

### Rust

`oneui-sys` 与 C ABI 一一对应。安全 `oneui` crate 负责：

- RAII 句柄和 callback drop 清理；
- UTF-8 字符串与结构化数组转换；
- UI 线程约束；
- `UiDispatcher` 将 `Send + 'static` 工作投递回窗口线程；
- worker-friendly `WidgetHandle` / `LabelHandle` / `TableHandle` 等合并更新；
- callback panic 捕获，禁止 unwind 穿过 C ABI；
- `InteractionTrace` 提供组件/事件/源代码位置诊断。

## 线程模型

- `Window::create` / Rust `Window::new` 在应用 UI 线程创建并初始化原生窗口；
- 消息循环、布局、绘制和控件 callback 在窗口线程执行；
- C++ 后台线程通过 `Window::post`；Rust 后台线程通过 `UiDispatcher::dispatch`；
- dispatcher 在窗口关闭后拒绝或取消待执行工作；
- `RealtimeFrameView::submitFrame` 保护最新帧状态，但 invalidation/presentation 仍由 UI 调度完成；
- 不允许调用方从后台线程直接修改普通原生控件对象。

## 所有权模型

### C++

- 组件以 `std::shared_ptr<Widget>` 组成树；
- `View`、容器、窗口和 overlay host 持有已挂载子项；
- callback 捕获应避免形成产品级共享环。

### C ABI

- `OneUiWindow*`、`OneUiWidget*` 等是 wrapper handle；
- `*_destroy` 释放 wrapper；
- 挂载 API 将底层 `shared_ptr` 交给树持有；
- 文档和测试允许调用方在挂载后销毁 wrapper，而树继续安全存在；
- callback `user_data` 的生命周期仍由调用方负责。

### Rust

- safe wrapper 拥有 C handle；
- callback storage 与 wrapper 同寿命，Drop 时先清除原生 callback；
- 将控件挂载到树后，Rust wrapper仍应在需要回调或直接控制时保持存活；
- 跨线程更新使用 handle，不把 UI-only wrapper 标记为任意线程可变。

## 布局与绘制生命周期

```text
state/API mutation
    -> invalidate / request layout
    -> platform schedules redraw
    -> root frame update and child layout
    -> paint tree using logical coordinates
    -> scale/present to physical pixels
```

布局树快照走同步诊断路径：

```text
request snapshot
    -> prepareLayoutSnapshot()
    -> commit the same layout/paint geometry used by presentation
    -> traverse mounted content + overlays
    -> serialize privacy-safe JSON
```

快照包含结构、frame、preferred frame、样式和语义摘要，不包含 TextField 明文值；测试同时覆盖
页面替换、modal overlay 生命周期与 VirtualList 精确 size-query/read 事务。

## 依赖方向红线

允许：

```text
product -> safe Rust / C ABI / public C++
C ABI -> public/core C++
platform -> core + Skia + OS APIs
core -> public neutral abstractions
```

禁止：

```text
core -> specific product
core -> Win32 messages/HWND
Rust safe API -> private C++ implementation
C ABI -> STL/C++ exceptions across boundary
platform -> product route/state
```

## 测试结构

CTest 当前包含 13 个目标：

- control behavior；
- selection model；
- overlay host；
- scroll view；
- stack；
- realtime frame view；
- remote input region；
- terminal view；
- tree view；
- panel；
- C ABI behavior；
- monitor behavior；
- backend contract。

Rust workspace另行验证 safe wrapper、dispatcher、callback 生命周期、panic 边界和结构化数据映射。

## 当前架构限制

- Win32 UI Automation bridge 尚未完成；现阶段是内部语义元数据，不应宣称系统读屏完整支持；
- StyleSheet 不是完整 CSS；
- Linux/macOS 没有消息循环、输入、DPI、字体、剪贴板和呈现实现；
- ABI 仍处于 0.x 版本收敛期；
- 布局 JSON 是结构/几何证据，不替代像素视觉测试；
- 产品仍负责异步业务、数据持久化、路由和错误恢复，OneUI 不接管业务状态机。
