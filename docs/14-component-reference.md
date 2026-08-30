# OneUI 组件参考

本文说明当前组件的用途、核心交互和采用边界。完整签名以公开头文件为准；语言覆盖见
[组件清单](07-component-inventory.md)。

## 共同约定

### 坐标与尺寸

- 所有控件 frame 和 preferred size 使用 OneUI 逻辑像素；
- Win32 后端负责 DPI 到物理像素的转换；
- `Widget::frame()` 是布局提交后的实际矩形；
- C ABI `oneui_widget_frame` 和 Rust `WidgetHandle::frame` 返回同一几何；
- 不要从截图反推布局，自动化诊断应使用 frame/布局 JSON，再用截图确认像素视觉。

### 可见、禁用与焦点

- invisible 控件不参与正常命中；
- disabled 控件不激活，tooltip 查询会回退到可用父级；
- `tabStop` 控制键盘遍历；
- `Window::requestFocus` / Rust `request_focus` 建立从根到后代的完整焦点链；
- focus-visible 与程序焦点分离，鼠标点击不应无条件显示键盘 focus ring。

### 样式

控件可通过 tag/class/pseudo state 使用 StyleSheet，也可使用 typed override。推荐顺序：

1. 应用 token；
2. 语义 class；
3. 控件 typed adapter；
4. 只有无法表达的动态值才使用直接 setter。

### 回调与受控数据

`onCloseRequested`、`onReorderRequested`、`onEditRequested`、`onDeleteRequested` 只报告用户
意图。产品数据是权威来源；持久化/业务更新成功后，再通过 `setItems` / `setRows` 提交新状态。

## Widget 与 View

`Widget` 是所有控件的基础，负责：

- frame / preferred size；
- visible / disabled / focused / tab stop；
- style node/classes；
- mouse/key/wheel/focus 事件；
- tooltip；
- accessible role/name/description/value/state；
- invalidation 与 animation scheduler。

`View` 是通用子树容器，增加：

- child ownership；
- 深层命中与焦点遍历；
- elevated child 绘制/输入；
- 深层 tooltip 查询；
- 后代 requestFocus。

普通页面应优先用语义容器（Stack/Panel/ScrollView/OverlayHost），只有自定义组合确实需要自由
frame 时才直接使用 View。

## Stack

一维行/列布局，适合绝大多数工具栏、表单和卡片内容。

```cpp
auto row = std::make_shared<oneui::Stack>(oneui::StackDirection::Row);
row->setPadding(oneui::Insets{8.0f, 12.0f});
row->setGap(8.0f);
row->setAlign(oneui::StackAlign::Center);
row->add(icon);
row->add(label);
row->add(action);
```

当前还可通过 `contentWidth()` / `contentHeight()` 查询 preferred children + gap + padding 的
内容范围，便于响应式判断。它不是完整 flexbox：没有 baseline、order、flex-shrink 或 wrapping。

## Panel

单内容 surface，提供背景、边框、圆角、阴影和 padding。Panel 适合卡片、侧栏、工具栏背景；
内部多项布局交给 Stack/Grid，不要在 Panel 内写产品专用坐标。

## ScrollView

单内容滚动 viewport：

- 显式内容宽高；
- 横/纵 offset 与最大 offset；
- 120 ms 滚轮 ease-out；
- 连续滚轮基于目标位置累加；
- 直接 setter、键盘和 thumb drag 保持即时；
- `scrollToBottom` 用于日志/终端附属视图；
- chrome 与 scrollbar style 可配置。

大数据列表不要把数千个 child 放入 ScrollView；请使用 VirtualList 或 Table。

## SplitView

双面板布局：

```cpp
auto split = std::make_shared<oneui::SplitView>(oneui::SplitOrientation::Horizontal);
split->setFirst(primary);
split->setSecond(inspector);
split->setRatio(0.74f);
split->setGap(1.0f);
split->setResizable(true);
split->setMinimumPaneExtent(560.0f, 320.0f);
split->setOnSplitRatioChanged([](float ratio) {
    // 高频拖拽预览；不要在这里同步写磁盘。
});
split->setOnSplitRatioCommitted([](float ratio) {
    // Pointer release / drag finish；适合持久化。
});
```

`changed` 可能连续触发；`committed` 表示本次拖拽结束。命中区会保证可操作宽度，不要求产品
通过扩大可见 gap 来改善拖拽。

## Grid / Wrap / ReorderableGrid

### Grid

固定列布局，适合已知列数的卡片。没有 CSS Grid span、`fr`、`minmax` 或 auto-fit。

### Wrap

自动换行排列，适合 tag/chip/小按钮。高级 line alignment 仍有限。

### ReorderableGrid

用于响应式卡片重排：

- 每个 item 提供稳定 ID；
- 列数可由 `grid-min-column-width` 随可用宽度减少；
- 拖拽阈值、插入位置和指示线由控件处理；
- internal reorder 只发 `(sourceId, targetIndex)`；
- external drag 发送 start/update/drop 与稳定 ID；
- 产品更新数据后调用 `moveItem` 或重新提交 items。

详细契约见 [重排契约](25-reorder-contract.md)。

## AppShell / ProductShell / WindowTitleBar

### AppShell

提供 sidebar/header/content/footer 四区、尺寸、padding/gap 和 sidebar visible。适合通用桌面壳。

### ProductShell

提供 sidebar/topbar/content/status slots 和一组纯几何 helper。它不包含具体产品 route 或数据。

### WindowTitleBar

自绘标题栏支持 icon、caption、minimize/maximize/close、variant 与 accessory：

```cpp
auto titleBar = std::make_shared<oneui::WindowTitleBar>(L"OneUI App");
titleBar->setAccessory(sessionTabs);
titleBar->setOnMinimize([&] { window->minimize(); });
titleBar->setOnMaximize([&] { window->toggleMaximize(); });
titleBar->setOnClose([&] { window->close(); });
```

无边框窗口若把 Tabs/Search 放入标题栏，还要配置窗口 interactive insets，使附件区域优先按
client 处理，其余区域仍可拖动窗口。

## Label / Button / IconButton

### Label

只读文本，支持文本绑定、字体、字重、颜色和对齐。长文本不会自动变成富文本或链接。

### Button

支持 primary/secondary/quiet/danger 等 variant、leading icon、trailing text、内容对齐、
hover/pressed/focus/disabled 状态、键盘 Enter/Space 激活和 callback。

### IconButton

紧凑图标操作。没有可见 label 时必须设置 tooltip 和 accessible name。

## InteractiveSurface

把任意内容变成语义化、可聚焦、可点击 surface：

- `setOnClick`；
- `setOnPointerActivated`（click count/modifier/坐标）；
- `setOnPointerMoved`；
- `setOnHoverChanged`；
- `setOnContextMenuRequested`；
- 可嵌套真正的 Button；键盘焦点会优先路由到深层交互 child，避免父子重复激活。

适合 metric card、时间线、可点击列表卡片，不要用它替代有专门语义的 Button/List/Table。

## TextField / TextArea

TextField 当前支持：

- 单行文本、选择、caret；
- undo/redo、剪贴板、Home/End、方向键；
- readonly、disabled、password mask；
- prefix/suffix icon；
- `onChanged` 与 `onSubmitted`；
- 样式和基本可访问性。

```cpp
auto path = std::make_shared<oneui::TextField>(L"/srv/releases");
path->setOnSubmitted([](const std::wstring& value) {
    openPath(value);
});
```

单行 Enter 只提交，不插入换行。TextArea 使用 multiline 模式和行高。富文本、语法高亮、
复杂 shaping 和完整 IME 产品矩阵不在当前承诺中。

## Checkbox / Switch / RadioGroup / Slider / Select

- **Checkbox**：二元勾选，适合独立布尔设置；
- **Switch**：即时生效开关，语义上不等于“表单提交时才生效”的 Checkbox；
- **RadioGroup**：少量互斥选项；
- **Slider**：连续/离散数值，支持步进、drag 和方向键；
- **Select**：单选下拉，支持键盘、light dismiss、Escape 和 viewport 内自动上下翻转。

Select 的 popup 能从嵌套 ScrollView/Dialog/OverlayHost 正确接收输入，并使用稳定 paint viewport，
不会因为 dirty clip 变小而错误放置。当前不支持多选、搜索或自定义 option widget。

## FormField / ValidationMessage

FormField 组合 label、required、helper/error 和 child：

```cpp
auto field = std::make_shared<oneui::FormField>();
field->setLabel(L"Host name");
field->setRequired(true);
field->setHelperText(L"Use a stable name visible to your team.");
field->setChild(hostNameInput);
```

它传播基础可访问性，但不管理整表提交、异步校验或数据收集。ValidationMessage 用于统一错误/
提示视觉。

## Tabs

Tabs 有两种 sizing：

- `Equal`：可用宽度等分；
- `Compact`：按文本/图标测量，受 min/max item width 限制，溢出时滚轮横向移动。

```cpp
auto tabs = std::make_shared<oneui::Tabs>();
tabs->setItems({L"SSH · production", L"Local shell", L"Logs"});
tabs->setItemIcons({oneui::IconSymbol::Terminal, std::nullopt, oneui::IconSymbol::Notebook});
tabs->setSizingMode(oneui::TabsSizingMode::Compact);
tabs->setItemWidthRange(96.0f, 220.0f);
tabs->setClosable(true);
tabs->setReorderEnabled(true);
tabs->setOnCloseRequested([](int index) { requestClose(index); });
tabs->setOnReorderRequested([](int source, int target) { requestMove(source, target); });
tabs->setOnContextMenuRequested([](int index, oneui::Point at) { openTabMenu(index, at); });
```

支持 pointer、Home/End、方向键、compact overflow wheel。close/reorder 不直接修改 items。

## List

小数据集列表，item 有 title/detail，支持 selected index 和 changed callback。可读取 `items()` 与
`itemFrame(index)`。它不虚拟化，不适合成百上千行。

## VirtualList

固定行高大列表：

- 只绘制可见行；
- scroll offset / max offset / row frame；
- 单选/多选与 Ctrl/Shift；
- changed/selection/activated/edit/delete/context callbacks；
- internal reorder 与 external stable-ID drag；
- 单行 `updateItem` 保持滚动和选择；
- Rust `VirtualListHandle::set_items` 可在线程间投递整表 revision 并合并更新。

详细内容见 [VirtualList](17-virtual-list.md)。

## TreeView

结构化 item 使用稳定 `id` 与 `parentId`，支持 title/detail、展开、选择、重排请求和外部 drop
target。产品必须保证 ID 唯一且层级无环。TreeView 不自动写产品树。

## Table

Table 是多列、固定行高的数据视图，不再是静态占位表格。

```cpp
auto table = std::make_shared<oneui::Table>();
table->setColumns({
    {L"Name", 160.0f},
    {L"Address", 180.0f},
    {L"Status", 0.0f},
});
table->setRows({
    {L"Production", L"10.0.0.8", L"Online"},
    {L"Staging", L"10.0.1.8", L"Offline"},
});
table->setRowHeight(32.0f);
table->setSelectionMode(oneui::SelectionMode::Multiple);
table->setOnActivated([](int row) { openRow(row); });
table->setOnEditRequested([](int row) { openEditor(row); });
table->setOnDeleteRequested([](const std::vector<int>& rows) { confirmDelete(rows); });
```

已实现：

- 结构化列和 row；
- `updateRow`；
- 可见行绘制与平滑滚动；
- 单/多选、Ctrl/Shift、键盘可见性；
- Enter 激活、F2 编辑请求、Delete 删除请求、右键 target；
- internal reorder request；
- external drag start/update/drop + stable ID；
- CSS hover/pressed/selected/scrollbar；
- C ABI 结构化 UTF-8 rows；
- Rust `TableHandle` 后台整表/单行更新。

未实现内置排序、筛选、列 resize、可变行高和单元格 editor。

## Menu / Popup / Dialog / OverlayHost

### Menu

header、item、separator、disabled、danger、activation，支持 `clearItems()` 重建动态菜单。

### Popup

保存 anchor/content/open/placement/interaction mode，通常由 OverlayHost 挂载。LightDismiss
允许外部点击关闭并继续产品输入；Modal 会阻断外部 pointer 并限制焦点。

### Dialog

提供标题、副标题、图标、内容、actions 与 close。对话框几何和 modal 行为由 OverlayHost 管理。

### OverlayHost

作为页面根层管理普通内容和 overlay。嵌套 overlay host 会将 elevated child 的命中/绘制正确
传播到祖先。不要通过产品级绝对 HWND popup 绕开该体系。

## Card / Badge / ProgressBar / Sparkline

- **Card**：标准 surface 容器；
- **Badge/IconBadge**：紧凑状态；
- **ProgressBar**：确定性 0..1 进度；
- **Sparkline**：非交互小型趋势线。

```cpp
auto trend = std::make_shared<oneui::Sparkline>();
trend->setValues({0.18, 0.22, 0.41, 0.35, 0.63, 0.58});
```

Sparkline clamp 非 finite/超界值，绘制中线、series 和末端点。复杂坐标轴、tooltip、多 series、
缩放和数据采样应由专门 chart 控件实现，而不是继续堆入 Sparkline。

## StateView / StatusStrip / Toast

- **StateView**：页面级空/错/加载/无结果，包含一个可选行动；
- **StatusStrip**：持续状态与主次操作；
- **Toast**：短时消息 surface；当前 toast queue、自动超时和堆叠策略由产品层管理。

## TerminalView

TerminalView 是渲染/输入视图，不负责 SSH/PTTY 协议。当前支持：

- 行列网格与局部 cell 更新；
- 宽字符/continuation；
- foreground/background、bold/dim/inverse/conceal、underline variants、strike、overline；
- block/bar/underline cursor 与 blinking；
- selection、select-all、copy-on-select、clipboard paste；
- viewport、滚轮行数、line number gutter；
- pointer/raw-key/text-input/focus/scroll callbacks；
- hyperlink hit/hover；
- mouse reporting 与辅助鼠标按钮动作；
- named monospace font、字号、行高、letter spacing 和 palette。

协议解析、scrollback、reflow 和会话生命周期由产品 terminal engine 管理。详见
[TerminalView](33-terminal-view.md)。

## LogView

结构化彩色行、append/replace/clear、字号/行高和 content height。适合中等规模日志；需要大规模
搜索、折叠和索引时应在产品数据层完成。

## RealtimeFrameView

保留最新一帧，不累计队列：

- 借用式 C/C++ 提交会复制 BGRA8888/RGBA8888，适合低频调用；
- ABI v18 的 owned submit 移交不可变像素所有权，替换、拒绝或销毁时恰好回调释放一次；
- C++ paint 共享最新不可变帧，不再为每次绘制深拷贝整张画面；
- Rust `RealtimeFrameViewHandle` 从 worker 提交并把突发更新合并为唯一最新帧；
- ActualSize/Fit/Fill/Stretch；
- `contentRect()` 供远程输入映射；
- frameId/timestamp/stride 元数据；
- submit 状态有 mutex 保护，所有权释放发生在锁外，允许安全重入。

NV12 仅有枚举，尚未转换；颜色空间和硬件纹理不在当前实现。owned submit 省去
OneUI 边界处的额外像素复制，但解码器是否直接产出目标像素格式仍由产品决定。

## RemoteInputRegion

把本地 pointer/raw-key 映射为远程协议容易消费的事件：

- window/content/normalized/remote position；
- Left/Right/Middle/X1/X2；
- virtual key/scan code/down-up/repeat/extended/modifier；
- `releaseAllInputs()`；
- focus loss 自动释放，避免远端卡键。
- safe Rust wrapper 会在 Drop 前清理回调，并保留 X1/X2、滚轮和 modifier 数据。

产品仍负责把 Win32/raw input 与远端注入协议完整接线和安全策略。

## Tooltip

`Widget::setTooltip` 与 `tooltipAt` 独立于较长的 accessible description。View 会返回命中点下
最深层的 visible + enabled child tooltip；若 child hidden/disabled，则回退到祖先。

Icon-only action 必须同时设置 tooltip 和 accessible name。

## 布局树快照

C ABI `oneui_window_layout_snapshot_utf8` 与 Rust `Window::layout_snapshot_json` / dispatcher 同步
提交当前布局，然后序列化：

- node/parent ID；
- tag/class；
- actual/preferred frame；
- visible/disabled/focused 等状态；
- resolved CSS box；
- Label/List/Tabs 等安全语义摘要；
- overlay/modal 子树。

TextField 明文不会进入 JSON，只记录长度等脱敏信息。size query 与 read 是一致事务，调用方应：

1. 空 buffer 查询 `required_len`；
2. 分配包含 NUL 的 buffer；
3. 立即读取；
4. 若窗口已变化则重新查询。

布局快照验证结构和几何；像素对齐仍需同视口截图比较。

## Rust handle 模式

Rust UI wrapper 是窗口线程对象。后台服务使用 handle：

- `WidgetHandle`：visible、preferred size、frame、tooltip；
- `LabelHandle`：合并文本更新；
- `TextFieldHandle`：投递文本；
- `ProgressBarHandle` / `SparklineHandle`：投递数值/样本；
- `VirtualListHandle` / `TableHandle`：整表 revision 与单行 patch；
- `TerminalViewHandle`：grid/frame/viewport 等高频更新。
- `RealtimeFrameViewHandle`：远程画面所有权移交与最新帧合并。

不要把 UI wrapper 当作任意线程可变对象；通过 dispatcher/handle 保持窗口线程约束。
