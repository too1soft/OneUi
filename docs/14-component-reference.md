# OneUI 组件参考

这份参考按“现在能用什么、怎么用、状态/样式/绑定怎么接、限制在哪里”组织。它不是最终 API 手册，而是面向新开发者的实用入口。

## 状态标记

| 标记 | 含义 |
| --- | --- |
| 已完成 MVP | 当前可以在普通示例中使用，仍可能有 API 收敛 |
| 部分完成 | 基本形态存在，但交互、样式或平台能力明显不足 |
| 进行中 | 头文件/API 已存在，但完整行为仍需验收 |
| 未开始 | 规划中，当前不要依赖 |

## Button

用途：触发命令，例如保存、取消、打开设置。支持 Primary/Secondary 视觉变体、点击回调、文本绑定、禁用状态和 typed style override。产品项目应通过 `buttonStyleOverrideFromStyleSheet(sheet, StyleNode{...})` 接入 `.button:hover` / `.button:active` / `.button:disabled` 等规则，不应自行翻译按钮状态颜色。

```cpp
auto save = std::make_shared<oneui::Button>(L"保存");
save->setVariant(oneui::ButtonVariant::Primary);
save->setOnClick([&] {
    vm.save();
});
```

限制：暂无 loading、SplitButton；更完整的 command model 尚未建立。（图标按钮已有独立组件 `IconButton`，见 `include/oneui/controls/icon_button.h`。）

## TextField

用途：单行文本输入。

```cpp
oneui::State<std::wstring> projectName{L"OneUI app"};

auto input = std::make_shared<oneui::TextField>(L"请输入项目名称");
input->bindText(projectName);
input->setOnChanged([&](const std::wstring& value) {
    vm.validateName(value);
});
```

已支持：

- `setText(...)` / `text()`。
- `setReadOnly(bool)` / `readOnly()`。只读态仍可获得焦点、移动 caret、选择文本和复制文本，但不会响应输入、粘贴、剪切、删除、撤销或重做。
- `caretIndex()` / `setCaretIndex(size_t)`。
- `setSelectionRange(start, end)`、`selectionStart()`、`selectionEnd()`、`hasSelection()`、`selectedText()`、`selectAll()`、`clearSelection()`。
- `copySelectionToClipboard(Clipboard&)`、`cutSelectionToClipboard(Clipboard&)`、`pasteFromClipboard(const Clipboard&)`。
- `setClipboard(std::shared_ptr<Clipboard>)` 后支持 `Ctrl+A/C/X/V`。
- Win32 `SystemClipboard`。
- `setPasswordMode(bool)` / `setPasswordMask(wchar_t)`。
- `Left` / `Right` / `Home` / `End` 移动 caret；按住 `Shift` 扩展选择区；`Delete` / `Backspace` 优先删除选择区。
- `undo()` / `redo()` 恢复文本、caret 和 selection，并沿用绑定 / `onChanged` 的一次变更通知语义。
- `TextFieldStyleOverride::readOnly`。只读视觉态由 OneUI 统一绘制，业务项目不得在应用层自己拼输入框内部区域。
- `textFieldStyleOverrideFromStyleSheet(sheet, StyleNode{...})` 可从 StyleSheet 生成输入框状态样式，包括 `:hover`、`:focus`、`:disabled`、`:read-only`。

限制：还不是完整文本编辑器；IME composition、Linux/macOS 剪贴板 bridge、Ctrl+Z/Ctrl+Y 快捷键接入、复杂文本测量和多行布局仍未完成。

## Card

用途：承载表单区块、日志区块、最近连接卡片、状态提示等 surface。默认支持背景、边框、圆角和阴影。

已支持：

- `setBackground(...)`、`setBorder(...)`、`setRadius(...)`、`setShadow(...)`。
- `setStyleBox(...)` / `clearStyleBox()`。可直接消费 StyleSheet resolved `StyleBox`，复用背景、边框、圆角、外阴影和 inset shadow。
- `cardStyleBoxFromStyleSheet(sheet, StyleNode{...})`。产品项目应通过这个入口接入 `.card` / `.status-strip` 等规则，不应在业务层复制 Card 绘制参数。

## ProductShell

用途：给桌面产品壳提供主窗口、标题栏、侧栏、顶部栏、内容区、footer、卡片和导航项的标准布局/样式契约。

已支持：

- `computeProductShellLayout(...)`、`computeProductSidebarLayout(...)`、`computeProductTopBarLayout(...)`、`computeProductAssistHomeLayout(...)` 等布局计算。
- `ProductShellStyle`：包含 `window`、`titleBar`、`sidebar`、`header`、`content`、`footer`、`card`、`navItem`、`selectedNavItem`、`statusStrip`。
- `productShellStyleFromStyleSheet(sheet)`：从 `.window`、`.titlebar`、`.sidebar`、`.topbar`、`.content`、`.footer`、`.card`、`.nav-item`、`.nav-item:selected`、`.status-strip` 解析样式。

## Select

用途：从少量选项中选择一个值。

```cpp
oneui::State<int> selectedPlatform{0};

auto platform = std::make_shared<oneui::Select>();
platform->setItems({L"Windows", L"Linux", L"macOS"});
platform->bindSelectedIndex(selectedPlatform);
platform->setOnChanged([&](int index) {
    vm.platform.set(index);
});
```

状态 / 样式 / 绑定：

- `setItems(std::vector<std::wstring>)` 设置选项。
- `setSelectedIndex(int)` / `selectedIndex()` 读写选中项。
- `bindSelectedIndex(State<int>&)` 绑定选中项。
- `setOnChanged(...)` 监听选择变化。
- `setStyleOverride(SelectStyleOverride)` 覆盖选择框本体、箭头、内置 popup、option 行、selected option 和 focus ring。
- 当 `setItems(...)` 缩短或清空选项后，当前有效选中项会归一化到新范围；如果有效值发生变化，会同步绑定状态并只触发一次 `onChanged`。
- 打开 dropdown 后，点击 field/dropdown 外部会关闭下拉，但不会改变 `selectedIndex`，也不会触发 `onChanged`；这次点击仍可继续派发给其它控件。
- 点击另一个 `Select` 时，前一个 `Select` 会先关闭，新的 `Select` 会打开。

当前实现说明：

- 当前是内置最小下拉实现，public API 未变。
- 内部 popup 状态已经收敛为私有 `LightDismissModel` / `PopupLightDismissReason`。
- popup 状态清理和 light-dismiss 行为基线已经验收。
- 下拉几何已经通过内部 adapter 复用 `PopupPlacement`，因此 offset、anchor 和 surface rect 的计算路线开始向共享 Popup 靠拢。

限制：

- 尚未真正挂载到共享 `OverlayHost` / `Popup`。
- 长列表滚动、typeahead、完整键盘 dropdown 行为和更完整可访问性仍未完成。

## Checkbox

用途：二元开关或多选项。

```cpp
oneui::State<bool> enabled{true};

auto checkbox = std::make_shared<oneui::Checkbox>(L"启用实时预览");
checkbox->bindChecked(enabled);
checkbox->setOnChanged([&](bool checked) {
    vm.livePreview.set(checked);
});
```

限制：暂无 `CheckboxGroup`；indeterminate 状态尚未实现。

## RadioGroup

用途：从一组互斥选项里选择一个。

```cpp
oneui::State<int> themeMode{0};

auto group = std::make_shared<oneui::RadioGroup>();
group->setItems({L"跟随系统", L"浅色", L"深色"});
group->bindSelectedIndex(themeMode);
```

支持鼠标和键盘选择，支持 `RadioGroupStyleOverride`。更细的布局方向、分组 label、帮助文本建议用 `FormField` 组合。

## Tabs

用途：在同一区域切换多个视图或分区。

```cpp
oneui::State<int> tab{0};

auto tabs = std::make_shared<oneui::Tabs>();
tabs->setItems({L"控件", L"数据", L"样式"});
tabs->bindSelectedIndex(tab);
```

Rust 应用使用安全包装层管理回调生命周期：

```rust
let mut tabs = oneui::Tabs::new(&[
    "生产堡垒机".to_string(),
    "Kylin V10".to_string(),
])?;
tabs.set_on_changed(|index| {
    // 切换与该索引关联的工作区内容。
});
```

C ABI 对应 `oneui_tabs_*`，标签通过结构化 UTF-8 数组传递。`SegmentedControl`
用于少量互斥模式，工作区、文档和会话切换应使用 `Tabs`，不要混用两种语义。

限制：当前只负责 tab bar，不自动管理对应 page 内容；暂无关闭按钮、可拖拽 tab、溢出菜单。

## List / ListItem

用途：展示小数据量的可选列表，例如项目列表、设置项、简单记录。

```cpp
auto list = std::make_shared<oneui::List>();
list->setItems({
    {L"OneUI Gallery", L"桌面控件示例"},
    {L"SDK Consumer", L"CMake 接入示例"}
});
list->bindSelectedIndex(selectedProject);
```

限制：不是虚拟列表，不适合大数据量；暂无多选、分组、图标、右侧操作区。

## StateView

用途：统一承载页面级空数据、无搜索结果、加载中和错误状态。组件负责居中布局、语义图标、标题、说明和一个可选操作，不应再由业务页面手工拼接临时 `Panel + Label + Button`。

```cpp
auto empty = std::make_shared<oneui::StateView>(
    L"还没有任何主机",
    L"新建主机，或从其他终端工具导入连接配置。");
empty->setIcon(oneui::IconSymbol::Server);
empty->setAction(L"新建主机");
empty->setOnAction([&] {
    openHostEditor();
});
```

标准样式节点为 `state-view`，内部语义类为 `.state-view-icon`、`.state-view-title`、`.state-view-message` 和 `.state-view-action`。产品可以追加 `.content-state-empty`、`.content-state-no-results`、`.content-state-loading` 或 `.content-state-error` 等状态类，但颜色、字号和间距应继续来自统一 CSS token。

Rust 封装提供同名 `StateView`，C ABI 使用 `oneui_state_view_*`。操作回调由调用方持有的 Rust 包装对象管理；只要控件仍在原生视图树中并需要回调，包装对象就必须保持存活。

## Table

用途：展示小型固定列数据，例如构建产物、状态列表、属性表。

```cpp
auto table = std::make_shared<oneui::Table>();
table->setColumns({
    {L"组件", 160.0f},
    {L"状态", 120.0f},
    {L"说明", 0.0f}
});
```

限制：不是 DataGrid；暂无排序、筛选、列 resize、虚拟滚动、单元格编辑。

## FormField

用途：给表单控件提供 label、必填标记、helper/error 文案和统一间距。推荐包装 `TextField`、`Select`、`Checkbox`、`RadioGroup` 等输入控件。

```cpp
auto field = std::make_shared<oneui::FormField>();
field->setLabel(L"项目标识");
field->setRequired(true);
field->setHelperText(L"只能使用小写字母和连字符。");
field->bindInvalid(keyInvalid);
field->setChild(keyInput);
```

`FormField` 也会把 label/helper/error/required/invalid 同步到子控件的基础可访问性语义；如果子控件已经显式设置 accessible name/description，则保留开发者设置。

限制：不是完整 Form 容器，不负责提交、字段收集或校验流程。

## PopupPlacement

用途：计算锚点浮层的位置。它只做几何，不负责打开、绘制、焦点、关闭或层级管理。

```cpp
oneui::PopupPlacementRequest request{
    oneui::Rect{20.0f, 170.0f, 80.0f, 24.0f},
    oneui::Size{120.0f, 60.0f},
    oneui::Rect{0.0f, 0.0f, 300.0f, 220.0f},
    oneui::PopupPreferredPlacement::BottomStart,
    6.0f
};

auto result = oneui::PopupPlacement::resolve(request);
```

可选方向包括 `BottomStart`、`BottomEnd`、`TopStart`、`TopEnd`、`LeftStart`、`RightStart`。上下方向会在垂直主轴溢出时翻转，左右方向会在水平主轴溢出时翻转；最终矩形仍会被 shift / clamp 回 viewport 内。

限制：需要和 `Popup` / `OverlayHost` 组合才是完整浮层。

## Popup

用途：承载一个锚点和一个浮层内容，面向 Select、Menu、Tooltip、Popover、Dialog 等后续能力。

```cpp
auto popup = std::make_shared<oneui::Popup>();
popup->setAnchor(button);
popup->setContent(menuContent);
popup->bindOpen(open);
popup->setPreferredPlacement(oneui::PopupPreferredPlacement::BottomStart);
popup->setOutsidePointerPolicy(oneui::PopupOutsidePointerPolicy::Close);
popup->setCloseOnEscape(true);
```

已支持：

- `bindOpen(State<bool>&)`。
- `setAnchor(...)` / `setContent(...)`。
- `setOutsidePointerPolicy(PassThrough | Close | Block)`。
- `setInteractionMode(Modeless | LightDismiss | Modal)`。
- `overlayOptions(layer)` 根据交互模式生成 `OverlayOptions`。
- `setCloseOnEscape(true)` 时，打开状态下 Escape 会关闭 Popup 并消费这次按键；在 `OverlayHost` 中，已聚焦的 Popup 可以接收这次按键。

限制：完整弹层体系仍在进行中；嵌套层级、动画、modal 遮罩绘制和 Popup 内部多子控件焦点循环还需继续补。

## OverlayHost

用途：在普通 View 子树之上挂载 overlay，并按 layer 绘制和命中测试。它是 Popup/Menu/Dialog/Toast 等能力的基础设施。

```cpp
auto host = std::make_shared<oneui::OverlayHost>();
host->add(content);
host->addOverlay(popup, oneui::theme().layerPopup);
host->addOverlay(dialog, oneui::OverlayOptions::modal(oneui::theme().layerModal));
```

已支持：

- `addOverlay(child, layer)` 添加普通 overlay。
- `addOverlay(child, OverlayOptions{layer, trapsFocus, blocksOutsidePointer})` 添加带选项的 overlay。
- `OverlayOptions::modeless(layer)` 和 `OverlayOptions::modal(layer)` 薄预设。
- `removeOverlay(const Widget*)`、`clearOverlays()`、`overlays()`。
- 挂载、移除、层级排序、基础事件转发。
- `trapsFocus` 键盘焦点边界。
- `blocksOutsidePointer` 外部指针阻断。
- overlay-to-overlay 焦点历史，以及最后一个聚焦 overlay 关闭后返回普通 View 焦点。

限制：

- `Select` 还没有迁移到共享 `OverlayHost` / `Popup`。
- 仍需更完整的 Popup/Menu/Dialog/Toast 组合验收、统一 portal API、modal 遮罩绘制、Dialog 外观、toast 队列和 Popup 内部多子控件焦点循环。

## 远程会话基础组件

这一组 API 是为了让远程控制客户端尽快接入 OneUI。它们不是普通表单控件，而是远程会话窗口里最先需要的底座：实时画面、输入映射、显示器/DPI 和窗口生命周期。

### RealtimeFrameView

用途：承载远程核心解码出来的最新视频帧，并为输入坐标映射提供稳定的 `contentRect()`。

```cpp
auto frameView = std::make_shared<oneui::RealtimeFrameView>();
frameView->setScaleMode(oneui::ScaleMode::Fit);

oneui::VideoFrame frame;
frame.data = decodedBgraBytes;
frame.width = 2560;
frame.height = 1440;
frame.stride = frame.width * 4;
frame.format = oneui::PixelFormat::Bgra8888;
frame.frameId = nextFrameId;
frame.timestampUs = captureTimestampUs;

frameView->submitFrame(frame);
auto visibleVideoRect = frameView->contentRect();
```

已支持：

- `PixelFormat::Bgra8888` / `Rgba8888` 的内部拷贝。
- `ScaleMode::ActualSize` / `Fit` / `Fill` / `Stretch`。
- `VideoFrame` 元数据：`frameId`、`timestampUs`、`stride`。
- 只保留最新帧；新帧覆盖旧帧，不做队列堆积。
- `latestFrame()` 返回快照，主要用于测试和调试。

限制：

- 当前 `paint()` 只绘制占位背景，还没有把像素真正 blit 到窗口。
- `Nv12` 只是 API 预留，尚未拷贝或解码。
- `submitFrame()` 内部状态有 mutex 保护，但 `invalidate()` 仍沿用现有 Widget invalidator；完整 UI 线程 dispatcher 还需要继续接。

### RemoteInputRegion

用途：把远程会话区域的 pointer 和 raw key 抽象成远端注入协议容易消费的事件，同时负责窗口坐标到画面坐标、归一化坐标、远端屏幕坐标的映射。

```cpp
auto input = std::make_shared<oneui::RemoteInputRegion>();
input->setRemoteSize(oneui::Size{1920.0f, 1080.0f});
input->setScaleMode(oneui::RemoteInputScaleMode::Fit);

input->setOnPointer([&](const oneui::RemotePointerEvent& event) {
    remoteSession.sendPointer(event.remotePosition, event.button, event.pressed);
});

input->setOnRawKey([&](const oneui::RawKeyEvent& event) {
    remoteSession.sendKey(event.virtualKey, event.scanCode, event.pressed);
});

// 断开或停止时仍应显式调用，避免远端卡键。
// 组件失焦时会自动释放已按下输入。
input->releaseAllInputs();
```

已支持：

- `PointerButton::Left` / `Right` / `Middle` / `X1` / `X2` / `None`。
- `RemotePointerEvent` 包含 `windowPosition`、`contentPosition`、`normalizedPosition`、`remotePosition`。
- `RawKeyEvent` 包含 `virtualKey`、`scanCode`、down/up、repeat、extended、alt/ctrl/shift/win。
- `dispatchPointer(...)` 和 `dispatchRawKey(...)` 可由平台层或业务层显式投递。
- `releaseAllInputs()` 会释放已按下的 pointer button 和 raw key。
- `onFocusChanged(false)` 会自动调用释放逻辑，防止窗口失焦导致远端卡键。

限制：

- 当前 Win32 `Widget` 鼠标事件仍只能自然带出左键；右键、中键、XButton、raw key up、scan code 等原生消息接入还在 P0 后续任务中。
- `onKeyDown()` 暂不转发语义化 `KeyEvent`，避免和文本输入重复；远程输入应走 `RawKeyEvent`。

### Monitor / DPI

用途：远程窗口需要知道本机显示器的 bounds、work area、DPI scale，尤其要保留负坐标副屏，后续才能稳定处理跨屏窗口和坐标映射。

```cpp
for (const oneui::MonitorInfo& monitor : oneui::enumerateMonitors()) {
    std::wcout << monitor.name
               << L" scale=" << monitor.scale
               << L" bounds=(" << monitor.bounds.x << L"," << monitor.bounds.y << L")\n";
}
```

已支持：

- `MonitorInfo`：`index`、`bounds`、`workArea`、`scale`、`primary`、`name`。
- Win32 后端通过 `EnumDisplayMonitors` / `GetMonitorInfoW` 枚举显示器。
- 动态加载 `Shcore.dll` 的 `GetDpiForMonitor`；旧系统或 API 不存在时回退到 1.0 scale。
- bounds/workArea 使用 `Rect`，不会丢掉负坐标。

限制：

- 还没有 DPI 变化事件。
- 还没有把窗口跨屏移动后的 DPI/坐标变化自动通知给 `Window` 或 `RealtimeFrameView`。

## 布局原语

### Stack

一维行/列布局。限制：无 flex-grow/flex-shrink、baseline、复杂 wrapping。

### Grid

固定列数网格。限制：无 span、`fr`、`minmax`、auto-fit。

### ReorderableGrid

固定列卡片网格，负责子项布局、拖拽阈值、插入位置和指示线绘制。每个子项必须提供稳定 ID；控件只报告 `(sourceId, finalTargetIndex)`，不会擅自修改产品数据。产品持久化成功后调用 `moveItem` 原位应用顺序，避免重建整棵控件树。

`gap`、`height`（单项高度）、`padding`、`outline-color` 和 `outline-width` 由 CSS-like 样式控制。列数属于容器布局策略，通过 API 设置。详细契约见 [`docs/25-reorder-contract.md`](25-reorder-contract.md)。

### Wrap

自动换行排列 chip/tag/button。限制：对齐和尺寸策略仍很简单。

### DockView

桌面应用壳。限制：区域尺寸策略和样式 helper 仍需增强。

### SplitView

双面板。限制：不可拖拽，只有静态比例。

### ScrollView

用途：滚动 viewport。滚轮输入使用统一的 120 ms ease-out 过渡；连续滚轮会按目标位置累积，
而 `setScrollOffset`、键盘滚动和滚动条拖拽保持即时，便于精确定位。

```cpp
auto scroll = std::make_shared<oneui::ScrollView>();
scroll->setContent(widePanel);
scroll->setContentWidth(1600.0f);
scroll->setContentHeight(1400.0f);
scroll->setWheelStep(48.0f);
scroll->setHorizontalScrollOffset(80.0f);
```

状态 / API：

- `setContent(...)` 设置内容控件。
- `setContentWidth(float)` / `setContentHeight(float)` 设置内容尺寸。
- `setScrollOffset(float)` / `scrollOffset()` 管理纵向偏移。
- `setHorizontalScrollOffset(float)` / `horizontalScrollOffset()` 管理横向偏移。
- `maxScrollOffset()` / `maxHorizontalScrollOffset()` 读取最大偏移。
- 鼠标滚轮驱动纵向偏移。
- 水平内容溢出时会绘制水平 thumb，鼠标按住 thumb 拖拽会更新横向偏移并自动 clamp。
- 键盘 `Up` / `Down` / `Home` / `End` / `Left` / `Right` 已有基础处理。

限制：

- 垂直 thumb 是简单绘制，不支持拖拽；水平 thumb 也还没有可配置样式。
- 触控板精细 delta、惯性滚动、滚动条样式化和完整可访问性仍未完成。

## 其他已存在的控件

| 组件 | 状态 | 说明 |
| --- | --- | --- |
| `Label` | 已完成 MVP | 只读文本，支持文本绑定 |
| `Card` | 已完成 MVP | surface 容器，默认阴影已复用 `drawBoxShadow` |
| `Switch` | 已完成 MVP | 二元开关，语义接近 Checkbox |
| `Slider` | 已完成 MVP | 数值绑定、范围、步进、拖拽、键盘箭头和 `SliderStyleOverride` |
| `ProgressBar` | 已完成 MVP | 进度值展示，支持 `ProgressBarStyleOverride` |
| `Badge` | 已完成 MVP | neutral/success/warning/danger/accent 徽标，支持 `BadgeStyleOverride` |
| `Separator` | 已完成 MVP | 水平/垂直分隔线，支持 `SeparatorStyleOverride` |

## 还不要当成生产级的能力

- IME 和复杂文本编辑。
- 完整 Accessibility。当前 `Widget` 基础语义 API 已有，Button/TextField/Checkbox/Select/Slider/RadioGroup/Tabs/List/Table 已有默认语义；更多控件默认语义、语义树和平台 bridge 仍待实现。
- 完整 Popup/Menu/Dialog/Tooltip/Toast。
- 可变行高的大数据 DataGrid。固定行高的简单数据集请使用 `VirtualList`；它只绘制可见行并
  共享标准滚轮过渡，详细约定见 `docs/17-virtual-list.md`。
- Linux/macOS 目前只有平台骨架，不是可运行后端。
- 稳定应用启动封装。
- 完整视觉快照测试。
