# OneUI 架构

## 总体结构

```text
Application
  -> Platform Window
    -> Widget Tree
      -> Layout
      -> Event Dispatch
      -> Paint
```

OneUI 的核心层负责 `Widget`、`View`、布局、样式、状态和控件行为。平台层负责窗口、事件循环、DPI、剪贴板、IME、光标、最终像素呈现和系统集成。

当前可用平台后端是 Win32。Linux 与 macOS 目录代表后续后端方向，但尚未实现为可用后端。

## 代码分层

```text
include/oneui/
  geometry.h            Rect, Point, Size, Insets
  color.h               Color
  canvas.h              Canvas drawing abstraction
  widget.h / view.h     Widget/View base and event types
  reactive.h            State<T> / Binding<T> (minimal MVVM)
  style*.h              style.h / style_adapter.h / style_sheet.h / style_transition.h (typed CSS-like styling)
  material3_tokens.h    Material 3 color / state-layer / elevation tokens
  icon.h clipboard.h animation.h
  oneui_c_api.h         Stable C ABI (for Go/Rust/remote callers)
  controls/             Public control APIs (Button, TextField, Select, Popup, Toast, IconButton, ...)
  layout/               Layout + shells (Stack, Grid, Wrap, DockView, SplitView, ScrollView, Panel, OverlayHost, AppShell, ProductShell, ...)
  platform/             Public platform entry points (window.h, monitor.h)

src/core/
  Cross-platform widget, layout, state, style, and control behavior

src/platform/
  win32/            Implemented Win32 window and Skia raster presentation
  macos/            Future Cocoa backend skeleton
  linux/            Future X11/Wayland backend skeleton
```

## 渲染策略

v0 使用 Skia 作为主要渲染基础。在 Windows 上，平台后端创建 Skia raster surface，控件通过 `Canvas` 绘制到像素缓冲，再由 Win32 呈现到窗口。

这个选择是刻意保守的：

- 适合 Windows 7 时代的窗口呈现能力。
- 控件绘制不依赖 GDI 控件或系统主题。
- 未来 Linux 和 macOS 后端可以复用同一套核心控件绘制逻辑。

公开 API 通过 `Canvas` 隔离 Skia 头文件。平台和渲染后端可以直接使用 Skia，普通控件不应依赖 Skia 类型。

## Widget 模型

`Widget` 是所有控件的基础。它包含位置尺寸、可见性、禁用状态、焦点状态、绘制入口和输入事件入口。

`View` 是通用子树容器，负责子控件持有、命中测试、事件分发和焦点遍历。布局容器和业务视图都应优先建立在 `View` / `Widget` 之上。

`OverlayHost` 是浮层方向的共享基础，用于 Popup、Menu、Tooltip、Dialog、Toast 等控件。当前已有挂载、移除、层级、边界、事件转发，以及 focus trap / outside-pointer 策略（`OverlayOptions`）。`Popup`、`PopupPlacement`、`Toast` 已落地（含碰撞翻转、外部点击策略、Escape、基础阴影）；`Menu`、`Tooltip`、`Dialog` 仍是后续工作。

## 平台边界

平台代码可以：

- 创建原生窗口并运行事件循环。
- 把原生鼠标、键盘、文本、滚轮事件转成 OneUI 事件。
- 提供 `Canvas` 的平台呈现实现。
- 处理 DPI、剪贴板、IME、光标、文件对话框和可访问性桥接。

平台代码不应该：

- 拥有控件业务状态。
- 实现 Button、Select、TextField 等控件行为。
- 固化产品样式规则。
- 决定文本 shaping 策略。
- 引入终端用户必须安装的运行时依赖。

## 当前后端

Win32 后端当前承担真实运行路径：

- `CreateWindowExW` 创建窗口。
- 标准 Win32 message loop。
- `WM_PAINT` 触发 Skia raster 绘制与呈现。
- 鼠标、键盘、文本输入和滚轮消息转换为 OneUI 事件。
- 产品打包方向是 MSVC 静态运行时加 vendored static Skia。

