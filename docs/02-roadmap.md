# OneUI 路线图

## 当前状态

OneUI 已经不是只有第一个按钮的原型。当前核心方向是原生 C++ 桌面框架，Win32 后端可运行，Skia raster 负责绘制，核心控件通过 `Widget`、`View`、`Canvas`、`State<T>`、类型化 style override 和主题 token 逐步成形。

Linux 和 macOS 仍处于后端骨架阶段，不应在文档中描述为已完成。

## v0.1: First Light

目标：证明 OneUI 能打开原生桌面窗口并绘制自绘控件。

已覆盖范围：

- CMake 项目。
- 公开头文件。
- Win32 平台窗口。
- Skia-backed `Canvas`。
- `Widget` 基类。
- Button 控件。
- Gallery 可执行程序。

## v0.2: Core Model

目标：把控件树、布局、状态和基础样式模型打稳。

范围：

- `View` 子树与事件分发。
- `Stack`、`Grid`、`Wrap`、`DockView`、`SplitView`、`ScrollView` 等布局骨架。
- `State<T>` / `Binding<T>` 的 MVVM 绑定方向。
- 主题 token：颜色、圆角、间距、字体大小。
- 控件状态：hover、pressed、disabled、selected、focus-visible。
- 类型化 style override，作为 CSS-like 样式能力的第一步。

## v0.3: Forms And Input

目标：让常见表单和输入场景可用。

范围：

- `Label`、`TextField`、`Checkbox`、`Switch`、`RadioGroup`、`Slider`、`Select`。
- `FormField` 与 `ValidationMessage`。
- TextField 光标移动、基础文本输入和退格。
- 后续补齐选择、剪贴板、IME、密码模式和文本测量。

## v0.4: Floating UI

目标：建立浮层系统，让 Select、Menu、Tooltip、Dialog、Toast 等共享基础设施。

范围：

- `OverlayHost` 基础挂载、层级、边界和事件转发。
- `PopupPlacement` 纯几何定位。
- Popup 打开/关闭状态、外部点击关闭、Escape 关闭和焦点交接。
- Select 迁移到共享 Overlay/Popup 基础设施。

## v0.5: Renderer And Packaging Hardening

目标：让 SDK 走向可分发。

范围：

- vendored static Skia。
- MSVC `/MT` 静态运行时产品构建。
- 运行时导入审计。
- SDK 包含 headers、import lib、CMake package、docs、website 和 examples。
- 视觉快照测试和更强文本渲染路径。

## v1.0 最小承诺

v1.0 不追求巨大，但必须可信：

- 稳定核心 API。
- Windows 7+ 支持。
- Win32 后端成熟。
- Linux 和 macOS 后端达到可用状态。
- DPI-aware rendering。
- 可用 IME 和剪贴板。
- 键盘导航。
- 基础可访问性模型。
- 小而完整的高质量控件集。

