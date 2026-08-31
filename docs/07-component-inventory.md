# OneUI 组件清单

本文是当前工作区的组件与语言覆盖总表。它回答三个问题：

1. 组件现在是否真正实现；
2. C++、C ABI 和安全 Rust 是否都能使用；
3. 已知边界是什么。

权威签名仍以 `include/oneui`、`oneui_c_api.h` 和 Rust crate 为准。

## 标记说明

| 标记 | 含义 |
| --- | --- |
| 主线 | 有当前产品/示例和行为测试支撑，可用于 Win32 主线；不等同于稳定版 API 承诺 |
| 可用 | 核心行为可用，有明确高级能力缺口 |
| 实验 | API/测试已存在，但仍需要更多真实产品或平台验收 |
| ✓ | 该语言层有公开入口 |
| raw | 可从 `oneui-sys` 直接调用 C ABI，但安全 Rust 尚无同名包装 |
| — | 当前没有公开入口 |

## 基础运行时

| 能力 | 公开头文件 | 成熟度 | C++ | C ABI | safe Rust | 当前说明 |
| --- | --- | --- | --- | --- | --- | --- |
| Geometry / Color | `geometry.h`, `color.h` | 主线 | ✓ | POD | ✓ | Point/Rect/Size/Insets/Color 与逻辑像素基础 |
| Canvas | `canvas.h` | 主线 | ✓ | — | — | 图元、文字、渐变、阴影、clip、viewport、像素帧；主要由控件实现使用 |
| Widget | `widget.h` | 主线 | ✓ | ✓ | ✓ | frame、preferred size、disabled、visible、focus、class/style node、tooltip、语义 |
| View | `view.h` | 主线 | ✓ | 间接 | 间接 | 子树、深层命中、焦点链、tooltip 查询、elevated paint routing |
| Animation | `animation.h`, `style_transition.h` | 可用 | ✓ | 间接 | 间接 | easing、数值/颜色/StyleBox 过渡与 animation frame |
| State / Binding | `reactive.h` | 主线 | ✓ | — | Rust 自有状态 | 轻量订阅与绑定；C ABI 通过 setter/callback 表达 |
| SelectionModel | `selection_model.h` | 主线 | ✓ | 通过控件 | `SelectionMode` | 单/多选、Ctrl/Shift range、active/anchor |
| Icon | `icon.h` | 主线 | ✓ | ✓ | ✓ | 统一 IconSymbol 与 primitive；新增左右 Chevron |
| Clipboard | `clipboard.h` | 主线 | ✓ | ✓ | ✓ | Win32 系统文本剪贴板，UTF-8 与 UTF-16 兼容入口 |

## 布局与容器

| 组件 | 头文件 | 成熟度 | C++ | C ABI | safe Rust | 关键能力/限制 |
| --- | --- | --- | --- | --- | --- | --- |
| `Stack` | `layout/stack.h` | 主线 | ✓ | ✓ | ✓ | 行/列、gap、padding、align、flex child；可查询内容宽高；无完整 flexbox |
| `Grid` | `layout/grid.h` | 可用 | ✓ | — | — | 固定列网格；无 span/fr/minmax |
| `ReorderableGrid` | `layout/reorderable_grid.h` | 主线 | ✓ | ✓ | ✓ | 响应式列、稳定 ID、内部重排请求、外部拖拽；产品提交数据顺序 |
| `Wrap` | `layout/wrap.h` | 可用 | ✓ | — | — | 自动换行排列；高级对齐策略有限 |
| `Panel` | `layout/panel.h` | 主线 | ✓ | ✓ | ✓ | 单内容 surface、padding、背景、边框、圆角、阴影 |
| `ScrollView` | `layout/scroll_view.h` | 主线 | ✓ | ✓ | ✓ | 横纵偏移、滚轮平滑过渡、thumb、键盘滚动、内容尺寸 |
| `SplitView` | `layout/split_view.h` | 主线 | ✓ | ✓ | ✓ | 横/纵双栏、ratio、gap、最小范围、拖拽、changed/committed 回调 |
| `OverlayHost` | `layout/overlay_host.h` | 主线 | ✓ | ✓ | ✓ | 普通/anchored/modal overlay、层级、焦点、外部 pointer、嵌套越界命中 |
| `DockView` | `layout/dock_view.h` | 可用 | ✓ | — | — | 固定桌面区域组合；不是通用 docking system |
| `TopBar` | `layout/top_bar.h` | 可用 | ✓ | ✓ | raw | leading/actions/padding/gap；safe Rust 无专用包装 |
| `AppShell` | `layout/app_shell.h` | 主线 | ✓ | ✓ | raw | sidebar/header/content/footer 与响应式显示 |
| `ProductShell` | `layout/product_shell.h` | 可用 | ✓ | ✓ | raw | 产品工作台几何 helper 与 sidebar/topbar/status slots |
| `WindowTitleBar` | `controls/window_title_bar.h` | 主线 | ✓ | ✓ | ✓ | 自绘 caption 按钮、variant、interactive accessory、最大化状态 |

## 文本、按钮与表单

| 组件 | 头文件 | 成熟度 | C++ | C ABI | safe Rust | 关键能力/限制 |
| --- | --- | --- | --- | --- | --- | --- |
| `Label` | `controls/label.h` | 主线 | ✓ | ✓ | ✓ | UTF-8/UTF-16 文本、字体、颜色、对齐、绑定 |
| `Button` | `controls/button.h` | 主线 | ✓ | ✓ | ✓ | variant、图标、trailing text、状态样式、鼠标/键盘激活 |
| `IconButton` | `controls/icon_button.h` | 主线 | ✓ | ✓ | ✓ | 图标按钮、tooltip、focus、callback |
| `InteractiveSurface` | `controls/interactive_surface.h` | 主线 | ✓ | ✓ | ✓ | 自定义内容 surface、click、pointer activated/moved、hover、context menu |
| `TextField` | `controls/text_field.h` | 主线 | ✓ | ✓ | ✓ | 单行编辑、选择、撤销/重做、粘贴、只读、密码、前后图标、submit |
| `TextArea` | `controls/text_field.h` | 可用 | C++ multiline | ✓ | ✓ | multiline 与行高；复杂 IME/富文本不在当前范围 |
| SearchBox | C ABI 组合入口 | 可用 | 用 TextField 组合 | ✓ | 用 TextField 组合 | 搜索语义便捷构造，不是独立 C++ 类 |
| `Checkbox` | `controls/checkbox.h` | 主线 | ✓ | ✓ | ✓ | checked 状态、绑定/回调、键盘与默认语义 |
| `Switch` | `controls/switch.h` | 主线 | ✓ | ✓ | ✓ | 二元状态、绑定/回调 |
| `RadioGroup` | `controls/radio_group.h` | 可用 | ✓ | ✓ | raw | 横/纵布局、selected index、callback |
| `Slider` | `controls/slider.h` | 可用 | ✓ | — | — | min/max/step、drag、键盘、typed style；跨语言入口未补齐 |
| `Select` | `controls/select.h` | 主线 | ✓ | ✓ | ✓ | 单选、键盘、light dismiss、稳定 viewport 内翻转；非多选 combobox |
| `FormField` | `controls/form_field.h` | 可用 | ✓ | — | — | label、required、helper/error 与子控件语义传播 |
| `ValidationMessage` | `controls/validation_message.h` | 可用 | ✓ | — | — | 表单错误/提示文本 surface |
| `ButtonBridge` | `controls/button_bridge.h` | 可用 | ✓ | — | — | 产品自绘按钮几何/状态 helper，不是额外控件 |
| `TextInputBridge` | `controls/text_input_bridge.h` | 可用 | ✓ | — | — | 产品输入框布局 helper，不替代 TextField |

## 导航与数据组件

| 组件 | 头文件 | 成熟度 | C++ | C ABI | safe Rust | 关键能力/限制 |
| --- | --- | --- | --- | --- | --- | --- |
| `Tabs` | `controls/tabs.h` | 主线 | ✓ | ✓ | ✓ | equal/compact、内容测量、图标、overflow wheel、Home/End、close/context/reorder 请求 |
| SegmentedControl | C ABI 组合控件 | 可用 | 产品 helper | ✓ | ✓ | 少量互斥模式；不要代替工作区 Tabs |
| `NavItem` | `controls/nav_item.h` | 主线 | ✓ | ✓ | ✓ | sidebar 图标/文本/selected/click |
| `List` | `controls/list.h` | 主线 | ✓ | ✓ | ✓ | 小数据集、title/detail、单选与 frame 查询；非虚拟化 |
| `VirtualList` | `controls/virtual_list.h` | 主线 | ✓ | ✓ | ✓ | 固定行高可见行绘制、平滑滚动、单/多选、命令、重排、外部稳定 ID 拖拽 |
| `TreeView` | `controls/tree_view.h` | 主线 | ✓ | ✓ | ✓ | stable id/parent id、展开、选择、重排请求、外部 drop target |
| `Table` | `controls/table.h` | 主线 | ✓ | ✓ | ✓ | 结构化行列、可见行绘制、滚动、选择、激活、F2/Delete、context、reorder、外部 drag |
| `Menu` | `controls/menu.h` | 主线 | ✓ | ✓ | ✓ | header/item/separator/disabled/danger、动态 clear、activation |
| `Popup` | `controls/popup.h` | 主线 | ✓ | ✓ | ✓ | anchor/rect、placement、modeless/light-dismiss/modal、Escape、viewport clamp |
| `Dialog` | `controls/dialog.h` | 主线 | ✓ | ✓ | ✓ | 标题/副标题/图标/内容/操作/关闭，通常挂到 OverlayHost |
| `Tile` | `controls/tile.h` | 可用 | ✓ | ✓ | raw | title/subtitle/leading/trailing symbol/click |

### Table 当前没有内置的能力

- 排序和筛选模型；
- 列宽拖拽与列重排；
- 单元格 editor 或 data-binding adapter；
- 可变行高；
- 产品数据自动重排。

`onEditRequested` / `onDeleteRequested` / `onReorderRequested` 是命令请求，产品仍是数据源。

## 反馈、状态与视觉组件

| 组件 | 头文件 | 成熟度 | C++ | C ABI | safe Rust | 关键能力/限制 |
| --- | --- | --- | --- | --- | --- | --- |
| `Card` | `controls/card.h` | 主线 | ✓ | ✓ | raw | 内容 surface 与标准阴影 |
| `Badge` | `controls/badge.h` | 主线 | ✓ | ✓ | raw | neutral/success/warning/danger/accent |
| `IconBadge` | `controls/icon_badge.h` | 可用 | ✓ | ✓ | raw | 图标、accent、stroke width |
| `ProgressBar` | `controls/progress_bar.h` | 主线 | ✓ | ✓ | ✓ | 0..1 进度与样式；当前为确定性进度 |
| `Sparkline` | `controls/sparkline.h` | 主线 | ✓ | ✓ | ✓ | 0..1 sample、clamp、网格、折线、末端点；非交互图表 |
| `Separator` | `controls/separator.h` | 主线 | ✓ | — | — | 横/纵分隔和样式 |
| `StateView` | `controls/state_view.h` | 主线 | ✓ | ✓ | ✓ | 空/错/加载等状态的图标、标题、说明、操作 |
| `StatusStrip` | `controls/status_strip.h` | 可用 | ✓ | ✓ | raw | title/message/主次操作 |
| `Toast` | `controls/toast.h` | 可用 | ✓ | ✓ | raw | title/message/icon/actions/close；队列与自动超时由产品管理 |

## 专用视图

| 组件 | 头文件 | 成熟度 | C++ | C ABI | safe Rust | 关键能力/限制 |
| --- | --- | --- | --- | --- | --- | --- |
| `TerminalView` | `controls/terminal_view.h` | 主线 | ✓ | ✓ | ✓ | 网格/cell diff、宽字符、样式、选择、复制粘贴、光标、viewport、鼠标协议、超链接、raw key、行号 |
| `LogView` | `controls/log_view.h` | 主线 | ✓ | ✓ | ✓ | 结构化彩色行、append/replace/clear、等宽绘制、内容高度 |
| `RealtimeFrameView` | `controls/realtime_frame_view.h` | 可用 | ✓ | ✓ | ✓ | 最新帧覆盖、所有权移交完整帧、最多 64 个批量脏矩形、Rust worker 合并、BGRA/RGBA blit、Fit/Fill/Stretch/Actual；NV12 未实现 |
| `RemoteInputRegion` | `controls/remote_input_region.h` | 可用 | ✓ | ✓ | ✓ | pointer/raw-key、IME/Unicode committed text、可打印键去重、坐标归一化、远端坐标、release-all；远程协议注入与安全策略由产品负责 |

## 平台与系统能力

| 能力 | C++ | C ABI | safe Rust | 当前状态 |
| --- | --- | --- | --- | --- |
| Window create/show/run/close | ✓ | ✓ | ✓ | Win32 主线 |
| Borderless/fullscreen/topmost/resizable | ✓ | ✓ | ✓ | Win32 主线 |
| Placement round trip | ✓ | ✓ | ✓ | 恢复时验证可见 work area |
| Logical/pixel size + DPI | ✓ | ✓ | ✓ | 可查询，后端负责缩放 |
| Raw key | ✓ | ✓ | ✓ | 窗口级优先 callback，焦点丢失重置 modifier |
| Title-bar drag/interactive insets | ✓ | ✓ | ✓ | 用于标题栏中的 Tabs/Search 等可点击附件 |
| Clipboard | ✓ | ✓ | ✓ | 文本 |
| File/folder dialog | 平台方法 | ✓ | ✓ | owner-bound native dialog |
| Confirm / prompt | 平台方法 | ✓ | ✓ | UI-thread 与 blocking worker 形式 |
| Tray / notification | 平台实现 | ✓ | raw | show/hide/menu/notification |
| Monitor enumeration | ✓ | 平台内部/产品使用 | — | bounds/work area/scale/primary/name |
| Layout snapshot JSON | 平台诊断 | ✓ | ✓ | 同步 commit，脱敏，包含 overlays 和真实 frame |
| Tooltip presentation | ✓ | setter | setter | 深层 visible/enabled child 优先 |

## 样式覆盖

公开 StyleSheet 支持：

- tag、`.class`、组合 class 与伪状态；
- `:hover`、`:active`、`:focus`、`:disabled`、`:selected`、`:read-only`；
- custom property 和 `var(--token)`；
- background/color/border/radius/padding/gap/width/height/font/outline/opacity/shadow；
- scrollbar color/width、grid minimum column width、transition duration/easing；
- Button、TextField、Select、ProgressBar、Popup、List、Table、TreeView、InteractiveSurface、Card 等 typed adapter。

不支持浏览器 DOM、cascade inheritance、flex/grid CSS 语法、媒体查询或任意 CSS 属性。

## 可访问性状态

已实现：

- Widget role/name/description/value/state；
- Button、TextField、Checkbox、Select、Slider、RadioGroup、Tabs、List、Table 等默认语义；
- FormField 的 label/helper/error 传播；
- 布局 JSON 中的脱敏语义摘要。

未完成：

- Win32 UI Automation provider/语义树桥；
- 完整读屏、自动化客户端和平台 action pattern；
- 所有复合控件的 child-level 平台节点。

因此当前文档只声明“有可访问性语义元数据”，不声明“完整系统无障碍支持”。

## 测试覆盖

当前 13 个 CTest 目标覆盖：

- 通用控件、Tabs、Table、Sparkline、TextField、tooltip；
- SelectionModel；
- OverlayHost、Popup、嵌套 overlay 与 Select；
- ScrollView、Stack、Panel；
- RealtimeFrameView、RemoteInputRegion；
- TerminalView、TreeView；
- C ABI、布局树快照、显示器与 Win32 backend contract。

Rust workspace 另外覆盖 safe wrapper、结构化数组、handle 合并更新、回调 Drop、panic 捕获与
窗口线程 dispatcher。

## 采用建议

- 新产品页面优先使用“主线”组件；
- “可用”组件在采用前写清产品所需缺口并补行为测试；
- “实验”组件需要真实运行环境压力/输入验证；
- 大数据列表使用 VirtualList；多列数据使用 Table；层级数据使用 TreeView；
- 任何重排 callback 都视为请求，数据层成功后再更新 UI；
- 需要跨线程更新时使用 dispatcher/handle，不直接跨线程调用控件；
- 需要像素级布局审计时使用布局 JSON + 同视口截图，两者不能互相替代。
