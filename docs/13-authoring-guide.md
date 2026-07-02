# OneUI 编写指南

这份文档解释 OneUI 应用应该怎么写：布局怎么组织，样式怎么覆盖，CSS-like 在 OneUI 里具体意味着什么，MVVM 状态绑定怎么落地，以及事件回调和当前完成度如何判断。

## 1. 核心模型

OneUI 不是 HTML，也不是浏览器 CSS 运行时。当前核心模型是：

```text
Platform Window
  -> root View / OverlayHost
    -> layout containers
      -> controls
        -> State<T> / Binding<T>
        -> callbacks
        -> typed style overrides
```

| 概念 | OneUI 当前写法 |
| --- | --- |
| 页面根节点 | `View`、`Stack`、`DockView` 或 `OverlayHost` |
| 组件 | `std::make_shared<oneui::Button>(...)` |
| props | `setText(...)`、`setItems(...)`、`setRequired(...)` |
| v-model / MVVM binding | `bindText(state)`、`bindChecked(state)`、`bindSelectedIndex(state)` |
| 事件 | `setOnClick(...)`、`setOnChanged(...)` |
| children / slot | `add(...)`、`setChild(...)`、`setContent(...)` |
| class / style | `setStyleOverride(...)` |

## 2. 控件创建语法

```cpp
auto button = std::make_shared<oneui::Button>(L"保存");
button->setVariant(oneui::ButtonVariant::Primary);
button->setOnClick([&] {
    vm.save();
});
```

常见模式：

```cpp
auto control = std::make_shared<oneui::SomeControl>();
control->setXxx(...);
control->bindXxx(state);
control->setOnChanged(...);
control->setStyleOverride(style);
container->add(control);
```

建议把 ViewModel、视图构建、样式定义分开，不要把所有业务逻辑塞进控件构造代码里。

## 3. 布局语法

布局容器都是原生 C++ 类型，不使用 XML 或 HTML 模板。当前可用容器以简单、可预测为主。

### 3.1 Stack

`Stack` 用于一维布局：横向或纵向排列。

```cpp
auto stack = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
stack->setGap(12.0f);
stack->setPadding(oneui::Insets{16.0f});
stack->setAlign(oneui::StackAlign::Stretch);
```

已实现：方向、gap、padding、Start/Center/End/Stretch 对齐、跳过不可见子控件。

限制：还没有 flex-grow/flex-shrink、baseline 对齐和复杂 wrapping。

### 3.2 Grid

`Grid` 用于固定列数的二维布局。

```cpp
auto grid = std::make_shared<oneui::Grid>(2);
grid->setColumnGap(16.0f);
grid->setRowGap(12.0f);
grid->setPadding(oneui::Insets{16.0f});
grid->setAutoRows(56.0f);
```

限制：还没有 column span、row span、`fr` 轨道、`minmax`、auto-fit/auto-fill。

### 3.3 Wrap

`Wrap` 用于从左到右自动换行，适合标签、徽标、筛选 chip 和工具按钮组。

```cpp
auto wrap = std::make_shared<oneui::Wrap>();
wrap->setGap(8.0f);
wrap->setRowGap(8.0f);
wrap->setPadding(oneui::Insets{12.0f});
```

当前是 MVP skeleton，复杂对齐和尺寸控制仍待补齐。

### 3.4 DockView

`DockView` 适合典型桌面应用壳：

```cpp
auto dock = std::make_shared<oneui::DockView>();
dock->setTop(toolbar);
dock->setLeft(sidebar);
dock->setCenter(content);
dock->setBottom(statusBar);
```

### 3.5 SplitView

`SplitView` 用于左右或上下双面板。

```cpp
auto split = std::make_shared<oneui::SplitView>(oneui::SplitOrientation::Horizontal);
split->setSplitRatio(0.32f);
split->setFirst(fileTree);
split->setSecond(editor);
```

当前是静态 split，拖拽分割条尚未实现。

### 3.6 ScrollView

`ScrollView` 用于可裁剪 viewport，当前支持垂直滚动和已验收的水平偏移骨架。

```cpp
auto scroll = std::make_shared<oneui::ScrollView>();
scroll->setContent(widePanel);
scroll->setContentWidth(1600.0f);
scroll->setContentHeight(1200.0f);
scroll->setWheelStep(48.0f);
scroll->setHorizontalScrollOffset(120.0f);
```

已实现：

- 垂直滚动、鼠标滚轮、offset clamp、简单垂直 thumb。
- `setContentWidth(float)`、`horizontalScrollOffset()`、`setHorizontalScrollOffset(float)`、`maxHorizontalScrollOffset()`。
- 水平内容溢出时绘制水平 thumb，并支持鼠标按住 thumb 横向拖拽。
- 键盘骨架：`Up` / `Down` 调整纵向偏移，`Left` / `Right` 调整横向偏移，`Home` / `End` 跳到起点或最大偏移。

限制：

- 鼠标滚轮仍按垂直滚动处理。
- 垂直 thumb 仍是基础绘制，暂不支持拖拽。
- 精细触控板 delta、惯性滚动、滚动条样式化和完整可访问性语义仍未完成。

## 4. 样式系统

OneUI 不直接运行 CSS。当前设计目标是：

```text
CSS-like freedom, typed C++ API
```

也就是说：

- 全局视觉语义进入 `Theme`。
- 单个控件差异通过 `xxxStyleOverride`。
- hover、pressed、disabled、selected、focus-visible 等状态通过强类型 state override 表达。
- 控件级阴影复用 Canvas 的 `BoxShadow` / `drawBoxShadow(...)` 原语。

### 4.1 Select 状态样式

`SelectStyleOverride` 覆盖选择框本体、箭头、内置 popup surface 和 popup option。它仍然是当前的内置最小 dropdown，不代表已经迁移到共享 `OverlayHost`，但样式抽象已经和其他控件对齐。

```cpp
oneui::SelectStyleOverride style;

oneui::SelectStateStyleOverride normal;
normal.background = oneui::theme().surface;
normal.foreground = oneui::theme().text;
normal.border = oneui::theme().border;
normal.arrowColor = oneui::theme().textMuted;
normal.popupBackground = oneui::theme().surface;
normal.popupBorder = oneui::theme().borderStrong;
normal.padding = oneui::Insets{0.0f, 12.0f};
normal.popupOffset = 6.0f;

style.normal = normal;
select->setStyleOverride(style);
```

`setItems(...)` 会维护选中索引的有效范围。如果当前选中第 3 项，选项被替换成只剩 1 项，`Select` 会把选中项归一化为 `0`；如果绑定了 `State<int>`，状态也会同步更新，并且只触发一次 `onChanged(0)`。如果替换选项后有效索引没有变化，则不会重复触发 `onChanged`。

当前 `Select` 的 light-dismiss 是迁移共享 Overlay/Popup 前的行为基线：打开 dropdown 后，点击 field/dropdown 外部会关闭下拉，不改变 `selectedIndex`，不触发 `onChanged`；点击其它控件仍可继续派发；点击另一个 `Select` 会关闭前一个并打开后一个。内部状态已经收敛为私有 `LightDismissModel` / `PopupLightDismissReason`，public API 未变。下拉位置已经通过内部 adapter 复用 `PopupPlacement`，但这还不等于 `Select` 已经接入共享 `OverlayHost`。

## 5. MVVM 状态绑定

### 5.1 State

`State<T>` 是 ViewModel 中的可观察值。

```cpp
struct ProjectSettingsViewModel {
    oneui::State<std::wstring> name{L"OneUI app"};
    oneui::State<int> platform{0};
    oneui::State<bool> keyInvalid{false};
};
```

### 5.2 Binding

```cpp
auto name = std::make_shared<oneui::TextField>(L"项目名称");
name->bindText(vm.name);

auto platform = std::make_shared<oneui::Select>();
platform->setItems({L"Windows", L"Linux", L"macOS"});
platform->bindSelectedIndex(vm.platform);
```

当前 `Binding<T>` 持有 `State<T>` 的原始指针，因此 ViewModel 必须比所有绑定它的控件活得更久。

## 6. 事件回调

常见事件入口：

- `Button::setOnClick(std::function<void()>)`
- `TextField::setOnChanged(std::function<void(const std::wstring&)>)`
- `Checkbox::setOnChanged(std::function<void(bool)>)`
- `Select::setOnChanged(std::function<void(int)>)`
- `RadioGroup::setOnChanged(std::function<void(int)>)`
- `Tabs::setOnChanged(std::function<void(int)>)`
- `List::setOnChanged(std::function<void(int)>)`

注意：当前若有效值没有变化，选择类和值类控件正在避免重复触发 `onChanged`。后续还需要更多行为测试锁定所有控件的一致性。

## 7. 推荐项目结构

```text
my-app/
  CMakeLists.txt
  src/
    main.cpp
    viewmodels/
    views/
    styles/
```

建议职责：

- `viewmodels/`：状态、校验、业务动作。
- `views/`：创建控件树、绑定状态、连接事件。
- `styles/`：主题 token、组件 style override。
- `main.cpp`：窗口创建、root view、消息循环。

## 8. 完成度表

| 模块 | 状态 | 说明 |
| --- | --- | --- |
| Win32 窗口 | 已完成 MVP | 可创建窗口、处理基础输入和消息循环 |
| Skia raster 渲染 | 已完成 MVP | 当前 Win32 后端使用 Skia raster surface |
| MSVC 产品 SDK | 已完成 MVP | `oneui.dll`、`oneui.lib`、headers、CMake package、Gallery |
| `Widget` / `View` | 已完成 MVP | 子树、事件分发、焦点遍历、可见/禁用状态 |
| `State<T>` / `Binding<T>` | 已完成 MVP | 可用，但绑定生命周期模型仍需增强 |
| 布局容器 | 已完成 MVP | `Stack`、`Grid`、`Wrap`、`DockView`、`SplitView`、`ScrollView`；`ScrollView` 已有水平 content width/offset API 和键盘骨架 |
| 基础表单控件 | 部分完成 | `TextField` 已有 caret、选择区、copy/cut/paste、Win32 剪贴板 bridge 和 `undo()` / `redo()`；`Select` 内置 popup 状态清理和 light-dismiss 行为基线已验收，但仍缺共享 overlay、长列表/typeahead/完整键盘行为 |
| 样式系统 | 部分完成 | 多个控件已有 typed override，统一 CSS-like DSL 未完成 |
| `PopupPlacement` | 已验收窄切片 | 只负责几何定位 |
| `Popup` | 进行中 | 有基础 API 和交互预设，完整弹层行为仍需完善 |
| `OverlayHost` | 进行中 | 挂载、移除、层级、事件转发、焦点边界和外部指针阻断已有行为测试 |
| Accessibility | 部分完成 | 基础语义 API 和常用控件默认语义已有，语义树和平台 bridge 未完成 |
| Linux / macOS | 骨架存在，未实现 | 尚不是可运行后端 |
| 文档 / 网站 | 进行中 | 中文入门、作者指南、组件参考和静态入口持续补齐 |
