# OneUI 组件清单

本文档记录 OneUI 当前已有能力、重要缺口和后续组件目录。快照更新于 2026-07-01；条目如与代码不一致，一律以 `include/oneui/`、`src/core/` 的实际头文件/实现为准。

## 当前代码快照

已实现核心：

- `Widget`：frame、preferred size、disabled/visible binding、焦点标记、invalidation、基础可访问性语义 API。
- `View`：保留式子树、事件分发、焦点遍历、visible/disabled guard。
- `State<T>` 和 `Binding<T>`：最小 MVVM 状态与 scoped subscription。
- `Canvas`：clear、clip stack、圆角矩形 fill/stroke、ellipse、line、simple text。
- `Theme`：基础颜色、圆角、字体大小，以及 disabled/hover/pressed/selected 的背景、前景、边框状态 token。
- Style MVP：`ControlState`、`FocusRingStyle`、Button/TextField/Slider/ProgressBar/Select/Checkbox/RadioGroup/Tabs/Badge/Separator/List/Table/FormField/ValidationMessage/Popup style override。
- Style adapter：`buttonStyleOverrideFromStyleSheet(...)`、`textFieldStyleOverrideFromStyleSheet(...)`、`cardStyleBoxFromStyleSheet(...)` 和 `productShellStyleFromStyleSheet(...)`，用于把 CSS-like selector/state 映射到内置控件和产品外壳区域样式，业务项目不得自行翻译基础控件状态样式。
- Win32 后端：原生窗口、message loop、鼠标/键盘/文本分发、Skia raster canvas。
- 产品打包：MSVC 静态运行时 `oneui.dll`、import library、headers、CMake package、SDK smoke test、runtime import audit。

已实现布局：

- `Stack`：row/column、gap、padding、start/center/end/stretch alignment，跳过 invisible child。
- `Grid`：固定列数、行列 gap、padding、auto rows，跳过 invisible child。
- `ReorderableGrid`：面向产品卡片的响应式布局，使用稳定 item ID 报告鼠标拖拽重排；API 设置最大列数，`grid-min-column-width`、`gap`、`height`、`padding` 和插入指示线由 CSS-like 样式驱动，产品确认后再原位应用顺序。
- `Wrap`：左到右换行骨架。
- `DockView`：top/right/bottom/left/center app-shell 区域骨架。
- `SplitView`：横向/纵向双栏、静态或可拖拽比例、双侧最小尺寸、调整光标和比例变更回调。
- `ScrollView`：垂直滚动 viewport、鼠标滚轮、clipping、offset clamp、水平 content width / offset API、键盘滚动骨架、水平 thumb 绘制和水平 thumb 鼠标拖拽。

已实现控件：

- `Button`：primary/secondary、click、disabled、visible、`bindText`、typed style override。
- `Label`：只读文本、颜色、尺寸、对齐、`bindText`。
- `Card`：surface container，带边框，默认阴影已改为复用 `Canvas::drawBoxShadow(...)`，也可直接接收 `StyleBox` 以复用 StyleSheet 的背景、边框、阴影和 inset shadow。
- `TextField`：文本绑定、placeholder、caret index、基础选择区、只读态、密码显示模式、横向滚动裁剪、typed style override、copy/cut/paste API、Ctrl+A/C/X/V、按 caret/选择区插入、Left/Right/Home/End/Delete/Backspace、`undo()` / `redo()` 历史入口。
- `FormField`：label、required marker、helper/error text、invalid state、child layout、style override。
- `ValidationMessage`：独立 helper/error message，支持 binding、tone 和 style override。
- `Checkbox`、`Switch`：布尔输入。
- `Slider`：数值输入，支持范围、步进、拖拽、键盘箭头和 typed style override；`ProgressBar`：数值展示，支持 typed style override。
- `Tabs`、`RadioGroup`、`Select`：单选类控件；三者均支持 typed style override。
- `Badge`、`Separator`、`List`、`ListItem`、`TreeView`、`Table`：基础展示控件；`Badge`、`Separator`、`List`、`TreeView` 和 `Table` 支持 typed style override。`VirtualList` 与 `TreeView` 支持指针拖拽和 `Alt+方向键` 重排请求，并保持选择状态不被框架擅自修改。
- `OverlayHost`：浮层挂载、移除、层级、边界、基础事件转发、`OverlayOptions::trapsFocus` 键盘焦点边界、`OverlayOptions::blocksOutsidePointer` 外部指针阻断、`OverlayOptions::modal/modeless` 薄预设、overlay-to-overlay 焦点历史和普通 View 焦点恢复。
- `PopupPlacement`：浮层几何定位 resolver，支持 Bottom/Top/Left/Right 的 start/end 或 start 侧向定位、主轴 flip、shift 和 clamp。
- `Popup`：anchor/content 命中、鼠标焦点交接、键盘转发、`open/bindOpen`、`PopupOutsidePointerPolicy` 外部指针三态策略、`PopupInteractionMode` 交互模式预设、Escape 关闭和基础阴影。

## 重要缺口

- `Select`：已有最小内部 dropdown，内部 popup 状态清理和 light-dismiss 行为基线已经验收，下拉几何已通过内部 adapter 复用 `PopupPlacement`；仍缺真正挂载到共享 `OverlayHost` / `Popup`、长列表滚动、typeahead、可访问性和完整键盘 dropdown 行为。
- 焦点样式：已有 focus-visible 路径，仍需更完整的主题级 outline token。
- Gallery 焦点模型：自定义 sidebar/header target 与 `View` 子树焦点遍历仍未统一为应用级 focus scope。
- 阴影：`Canvas` 已有 `BoxShadow` / `drawBoxShadow(...)` 原语，`Card` 和 `Popup` 已接入；Dialog、Menu、Tooltip 等层级控件仍待逐个接入。Win32 当前是稳定 offset/spread 绘制，完整 blur 仍待后续兼容。
- 布局：缺 responsive grid span、draggable splitter、垂直 thumb 拖拽、滚动条样式化、触控板精细 delta 和窗口安全 popup placement 集成。
- 文本输入：已有基础 selection、密码显示模式、横向滚动裁剪、剪贴板抽象、Win32 系统剪贴板 bridge、Ctrl+A/C/X/V 行为和 `undo()` / `redo()`；仍缺 Linux/macOS 剪贴板 bridge、IME composition、Ctrl+Z/Ctrl+Y 快捷键接入和精确 text measurement。
- 文本渲染：`Canvas::drawText` 仍是 simple text，不足以支持复杂脚本、截断、换行和图文布局。
- 可访问性：`Widget` 已有基础 role/name/description/value/state API；Button/TextField/Checkbox/Select/Slider/RadioGroup/Tabs/List/Table 已有默认语义；FormField 已能把 label/helper/error/required/invalid 同步到子控件；仍缺更多控件默认语义、语义树、keyboard command model 和平台 accessibility bridge。
- 跨平台：只有 Win32 后端可用。Linux 和 macOS 有 skeleton，但不是已实现后端。
- 文档与网站：需要继续保持中文优先，并在公开网站中提供必要英文内容；旧文档中的损坏双语文本应清理。
- 测试：行为测试已覆盖一部分 `onChanged` 语义、TextField caret/选择区/插入/copy/cut/paste/Ctrl 快捷键、Select 内部 popup 清理、OverlayHost 挂载/移除/层级/边界/事件转发；仍缺视觉快照、可访问性测试、更广泛交互状态测试和跨平台 smoke test。
- Binding 生命周期：`Binding<T>` 仍持有 `State<T>*`，ViewModel state 必须长于绑定控件。

## 设计系统原语

需要逐步补齐：

- 颜色 token：语义色、状态色、禁用色、焦点色、elevation 色。
- 字体 token：family、size、line height、weight、fallback policy。
- 间距 token：scale、density、component padding、layout gaps。
- 圆角 token：尺寸级别和组件覆盖。
- 边框 token：宽度、颜色、focus outline、validation outline。
- 阴影/elevation token：card、popup、dialog、menu、tooltip。
- motion token：duration、easing、reduced motion。
- layer token：base、dropdown、tooltip、modal、toast。
- 状态模型：hover、active、focus、focus-visible、disabled、selected、checked、invalid、loading。

## 控件目录

### Foundation

- `Widget`
- `View`
- `ThemeProvider`
- `OverlayHost`
- `Portal`
- `Icon`
- `Text`
- `Separator`
- `Spacer`
- `Box`

### Buttons And Commands

- `Button`
- `IconButton`
- `SplitButton`
- `ToggleButton`
- `SegmentedControl`
- `CommandBar`
- `Toolbar`
- `MenuButton`
- `LinkButton`

### Form Inputs

- `TextField`
- `TextArea`
- `PasswordField`
- `SearchField`
- `NumberField`
- `Select`
- `ComboBox`
- `Autocomplete`
- `Checkbox`
- `CheckboxGroup`
- `RadioGroup`
- `Switch`
- `Slider`
- `RangeSlider`
- `Stepper`
- `DatePicker`
- `TimePicker`
- `ColorPicker`
- `FilePicker`
- `Form`
- `FormField`
- `ValidationMessage`

### Navigation

- `Tabs`
- `Breadcrumb`
- `SidebarNav`
- `TreeView`
- `Menu`
- `ContextMenu`
- `NavigationRail`
- `Pagination`
- `StepperNavigation`

### Data Display

- `Label`
- `Badge`
- `Tag`
- `Avatar`
- `Card`
- `List`
- `ListItem`
- `DescriptionList`
- `Table`
- `DataGrid`
- `TreeGrid`
- `CodeBlock`
- `Timeline`
- `Calendar`
- `EmptyState`

### Feedback

- `ProgressBar`
- `Spinner`
- `Skeleton`
- `Alert`
- `Toast`
- `Tooltip`
- `Popover`
- `Dialog`
- `Modal`
- `Drawer`
- `Banner`
- `InlineStatus`

### Surfaces And Layout

- `Panel`
- `Card`
- `GroupBox`
- `Accordion`
- `Disclosure`
- `Collapsible`
- `ResizablePanel`
- `ScrollView`
- `SplitView`
- `DockView`
- `WindowFrame`

## 迭代优先级

P0：让现有模型更连贯。

- 扩展 `onChanged`、interaction-state cleanup 和 focus-visible 行为测试。
- 用共享 overlay/popup 基础设施替换 `Select` 内部 dropdown。
- 补齐 theme-level focus outline token。
- 给 `Canvas` 和 `Theme` 增加 shadow/elevation。
- 扩展 `ScrollView` 到垂直 thumb 拖拽、触控板精细 delta、惯性滚动和可样式化滚动条。
- 文本测量与更可靠布局 sizing。
- `TextField` Linux/macOS 剪贴板 bridge、IME 和更完整文本编辑。

P1：让应用开发更实用。

- （`Popup`、`Toast`、`IconButton` 已落地：见 `src/core/{popup,toast,icon_button}.cpp`。）
- `Tooltip`、`Menu`、`Dialog`。
- `SegmentedControl`、`ComboBox`。
- 更完整表单组合与提交校验流程。
- 更实用的 `Table` / `DataGrid`。
- Linux/macOS 平台实现。

P2：走向产品级。

- IME、可访问性、DPI、视觉快照和行为测试。
- 跨平台后端 parity。
- motion token 和动画。
- 虚拟化与高级数据控件。
