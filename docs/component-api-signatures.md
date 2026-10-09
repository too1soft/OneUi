# 组件 API 签名速查

此页保留原官网的控件逐方法说明。当前能力与生命周期请同时参考[组件清单](07-component-inventory.md)与[组件参考](14-component-reference.md)，完整签名以包内公开头为准。

## 基础显示与容器

这些组件负责文本、图标、卡片、面板、分隔线和状态展示，是所有页面都会用到的基础积木。

### Label

显示一段只读文本，适合标题、说明、字段名和状态文案。

```cpp
#include <oneui/controls/label.h>
Label(std::wstring text = L"")
```

| API | 说明 |
|---|---|
| `setText(std::wstring) / text()` | 设置或读取显示文本。 |
| `bindText(State<std::wstring>&)` | 把文本绑定到响应式状态。 |
| `setColor(Color)` | 设置文字颜色。 |
| `setFontSize(float)` | 设置字号。 |
| `setFontWeight(int)` | 设置字体粗细。 |
| `setAlign(TextAlign)` | 设置文本对齐方式。 |
| `setTextOptions(TextOptions)` | Auto/LTR/RTL、语言标签 und 默认值、NoWrap/WordWrap。共享段落布局的完整发布验收仍在进行。 |
| `setMaxLines(int)` | 限制可见行数，保留每个硬段落的自动文字方向。 |

```cpp
auto title = std::make_shared<oneui::Label>(L"设备列表");
title->setFontSize(18.0f);
title->setFontWeight(600);
```

### IconView

绘制 OneUI 内置 IconSymbol，适合导航、状态、按钮前缀图标。

```cpp
#include <oneui/controls/icon_view.h>
IconView(IconSymbol symbol = IconSymbol::RemoteAssist)
```

| API | 说明 |
|---|---|
| `setSymbol(IconSymbol)` | 切换图标符号。 |
| `setColor(Color)` | 设置主线条颜色。 |
| `setAccent(Color)` | 设置强调色。 |
| `setStrokeWidth(float)` | 设置线条宽度。 |

```cpp
auto icon = std::make_shared<oneui::IconView>(oneui::IconSymbol::Monitor);
icon->setColor(oneui::Color{37, 99, 235, 255});
```

### Badge

显示短状态标签，例如 Online、Error、Beta、同步中。

```cpp
#include <oneui/controls/badge.h>
Badge(std::wstring text = L"", BadgeVariant variant = BadgeVariant::Neutral)
```

| API | 说明 |
|---|---|
| `setText(std::wstring) / text()` | 设置或读取标签文本。 |
| `bindText(State<std::wstring>&)` | 绑定文本状态。 |
| `setVariant(BadgeVariant)` | 设置 Neutral、Success、Warning、Danger、Accent 等语义色。 |
| `setStyleOverride(BadgeStyleOverride)` | 覆盖背景、文字、边框、圆角、内边距等样式。 |
| `clearStyleOverride()` | 恢复默认样式。 |

```cpp
auto badge = std::make_shared<oneui::Badge>(L"在线", oneui::BadgeVariant::Success);
```

### Separator

绘制水平或垂直分割线，用于分隔工具栏、表单区块和列表组。

```cpp
#include <oneui/controls/separator.h>
Separator(SeparatorOrientation orientation = SeparatorOrientation::Horizontal)
```

| API | 说明 |
|---|---|
| `setOrientation(SeparatorOrientation) / orientation()` | 设置水平或垂直方向。 |
| `setStyleOverride(SeparatorStyleOverride)` | 覆盖颜色、厚度、边距等。 |
| `clearStyleOverride()` | 恢复默认样式。 |

```cpp
auto line = std::make_shared<oneui::Separator>(oneui::SeparatorOrientation::Horizontal);
```

### ProgressBar

显示 0 到 1 范围内的进度，适合下载、连接、扫描、安装等过程。

```cpp
#include <oneui/controls/progress_bar.h>
ProgressBar()
```

| API | 说明 |
|---|---|
| `setValue(double) / value()` | 设置或读取进度值，通常会 clamp 到有效范围。 |
| `bindValue(State<double>&)` | 绑定进度状态。 |
| `setStyleOverride(ProgressBarStyleOverride)` | 覆盖轨道、填充、圆角等样式。 |
| `clearStyleOverride()` | 恢复默认样式。 |

```cpp
oneui::State<double> progress{0.35};
auto bar = std::make_shared<oneui::ProgressBar>();
bar->bindValue(progress);
```

### Card

带背景、边框、圆角、内边距和阴影的单内容容器。

```cpp
#include <oneui/controls/card.h>
Card()
```

| API | 说明 |
|---|---|
| `setContent(std::shared_ptr<Widget>)` | 设置卡片内部唯一内容。 |
| `setBackground(Color)` | 设置背景色。 |
| `setBorder(Color)` | 设置边框颜色。 |
| `setRadius(float)` | 设置圆角。 |
| `setPadding(Insets)` | 设置内容内边距。 |
| `setShadow(ControlShadowStyle)` | 设置阴影。 |
| `setStyleBox(StyleBox) / clearStyleBox()` | 通过样式盒统一覆盖视觉属性。 |

```cpp
auto card = std::make_shared<oneui::Card>();
card->setPadding(oneui::Insets{16});
card->setContent(content);
```

### Panel

通用面板容器，能力接近 Card，并额外支持边框宽度等布局场景。

```cpp
#include <oneui/layout/panel.h>
Panel()
```

| API | 说明 |
|---|---|
| `setContent(std::shared_ptr<Widget>)` | 设置面板内容。 |
| `setBackground(Color)` | 设置背景。 |
| `setBorder(Color, float width)` | 设置边框颜色和宽度。 |
| `setRadius(float)` | 设置圆角。 |
| `setPadding(Insets)` | 设置内边距。 |
| `setShadow(ControlShadowStyle)` | 设置阴影。 |

```cpp
auto panel = std::make_shared<oneui::Panel>();
panel->setPadding(oneui::Insets{20});
panel->setContent(form);
```

## 输入与选择

这些组件接收用户输入或修改状态。它们通常有 value、checked、selectedIndex、onChanged 或 onClick。

### Button

执行命令的标准按钮，支持主按钮、次按钮、禁用、键盘激活、点击回调和动画。

```cpp
#include <oneui/controls/button.h>
Button(std::wstring text)
```

| API | 说明 |
|---|---|
| `setText(std::wstring) / text()` | 设置或读取按钮文字。 |
| `bindText(State<std::wstring>&)` | 绑定按钮文字。 |
| `setVariant(ButtonVariant)` | 设置 Primary 或 Secondary。 |
| `setDisabled(bool)` | 设置禁用状态。 |
| `setOnClick(std::function<void()>)` | 设置点击回调。 |
| `setStyleOverride(ButtonStyleOverride)` | 覆盖 normal、hovered、pressed、disabled、selected、focusVisible 状态样式。 |
| `clearStyleOverride()` | 清除样式覆盖。 |

```cpp
auto save = std::make_shared<oneui::Button>(L"保存");
save->setVariant(oneui::ButtonVariant::Primary);
save->setOnClick([] { /* save */ });
```

### IconButton

只有图标的命令按钮，适合工具栏、标题栏、列表行操作。

```cpp
#include <oneui/controls/icon_button.h>
IconButton(IconSymbol symbol = IconSymbol::Monitor)
```

| API | 说明 |
|---|---|
| `setSymbol(IconSymbol) / symbol()` | 设置或读取图标。 |
| `setOnClick(std::function<void()>)` | 设置点击回调。 |
| `setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)` | 应用 CSS-like 样式，默认节点类似 button.icon-button。 |
| `setDisabled(bool)` | 继承 Widget 禁用能力。 |

```cpp
auto refresh = std::make_shared<oneui::IconButton>(oneui::IconSymbol::Refresh);
refresh->setOnClick(reloadDevices);
```

### Checkbox

布尔选择控件，适合“启用某项设置”或多选项。

```cpp
#include <oneui/controls/checkbox.h>
Checkbox(std::wstring text = L"")
```

| API | 说明 |
|---|---|
| `setText(std::wstring)` | 设置旁边的标签文本。 |
| `setChecked(bool) / checked()` | 设置或读取选中状态。 |
| `bindChecked(State<bool>&)` | 绑定布尔状态。 |
| `setOnChanged(std::function<void(bool)>)` | 状态变化时回调。 |
| `setStyleOverride(CheckboxStyleOverride)` | 覆盖多状态样式。 |
| `clearStyleOverride()` | 恢复默认样式。 |

```cpp
oneui::State<bool> remember{true};
auto checkbox = std::make_shared<oneui::Checkbox>(L"记住我");
checkbox->bindChecked(remember);
```

### Switch

开关控件，表达立即生效的开/关设置。

```cpp
#include <oneui/controls/switch.h>
Switch(std::wstring text = L"")
```

| API | 说明 |
|---|---|
| `setText(std::wstring)` | 设置说明文字。 |
| `setChecked(bool) / checked()` | 设置或读取开关状态。 |
| `bindChecked(State<bool>&)` | 绑定布尔状态。 |
| `setOnChanged(std::function<void(bool)>)` | 变化回调。 |
| `setStyleOverride(SwitchStyleOverride)` | 覆盖轨道、滑块、焦点等样式。 |
| `clearStyleOverride()` | 恢复默认样式。 |

```cpp
auto dark = std::make_shared<oneui::Switch>(L"深色模式");
dark->setOnChanged([](bool enabled) { applyTheme(enabled); });
```

### Slider

在连续或离散范围内选择数字值。

```cpp
#include <oneui/controls/slider.h>
Slider()
```

| API | 说明 |
|---|---|
| `setRange(double min, double max)` | 设置最小值和最大值。 |
| `setStep(double)` | 设置步长，0 或较小值可作为近似连续值。 |
| `setValue(double) / value()` | 设置或读取当前值。 |
| `bindValue(State<double>&)` | 绑定数值状态。 |
| `setOnChanged(std::function<void(double)>)` | 拖动或键盘调整时回调。 |
| `setStyleOverride(SliderStyleOverride)` | 覆盖轨道、填充、thumb 和焦点样式。 |

```cpp
auto volume = std::make_shared<oneui::Slider>();
volume->setRange(0.0, 100.0);
volume->setStep(5.0);
volume->setValue(60.0);
```

### TextField

单行文本输入控件，支持 placeholder、选择区、剪贴板、撤销重做、只读、密码模式和前后缀图标。

```cpp
#include <oneui/controls/text_field.h>
TextField(std::wstring placeholder = L"")
```

| API | 说明 |
|---|---|
| `setPlaceholder(std::wstring)` | 设置占位提示。 |
| `setText(std::wstring) / text()` | 设置或读取文本。 |
| `bindText(State<std::wstring>&)` | 绑定文本状态。 |
| `setCaretIndex(size_t) / caretIndex()` | 设置或读取光标位置。 |
| `textPosition() / setTextPosition(TextPosition)` | 新增 UTF-8 字节偏移与亲和方向接口；旧 caretIndex 仍使用本机宽字符下标。 |
| `setTextOptions(TextOptions)` | 方向 Auto/LTR/RTL、语言标签、换行模式；TextArea 默认不软换行，显式启用 WordWrap。 |
| `setSelectionRange(size_t start, size_t end)` | 设置选择范围。 |
| `selectionStart() / selectionEnd() / hasSelection()` | 读取选择区状态。 |
| `selectedText() / selectAll() / clearSelection()` | 读取、全选或清空选择区。 |
| `copySelectionToClipboard() / cutSelectionToClipboard() / pasteFromClipboard()` | 剪贴板编辑。 |
| `undo() / redo()` | 撤销和重做。 |
| `setReadOnly(bool) / readOnly()` | 设置或读取只读状态。 |
| `setPasswordMode(bool) / passwordMode()` | 启用密码显示。 |
| `setPasswordMask(wchar_t) / passwordMask()` | 设置密码掩码字符。 |
| `setPrefixIcon(std::optional<IconSymbol>) / setSuffixIcon(...)` | 设置前缀或后缀图标。 |
| `setOnChanged(std::function<void(const std::wstring&)>)` | 文本变化回调。 |
| `setStyleOverride(TextFieldStyleOverride)` | 覆盖 normal、hovered、disabled、readOnly、focusVisible 样式。 |

```cpp
auto name = std::make_shared<oneui::TextField>(L"请输入项目名称");
name->setPrefixIcon(oneui::IconSymbol::Search);
name->setOnChanged([](const std::wstring& value) { validate(value); });
```

### Select

从少量选项中选择一项，类似下拉选择框。

```cpp
#include <oneui/controls/select.h>
Select()
```

| API | 说明 |
|---|---|
| `setItems(std::vector<std::wstring>)` | 设置选项列表。 |
| `setSelectedIndex(int) / selectedIndex()` | 设置或读取当前索引。 |
| `bindSelectedIndex(State<int>&)` | 绑定选中索引。 |
| `setOnChanged(std::function<void(int)>)` | 选中项变化回调。 |
| `setStyleOverride(SelectStyleOverride)` | 覆盖 normal、hovered、pressed、disabled、selected、focusVisible 样式。 |

```cpp
auto mode = std::make_shared<oneui::Select>();
mode->setItems({L"自动", L"手动", L"只读"});
mode->setSelectedIndex(0);
```

### RadioGroup

从互斥选项中选择一项，适合模式、范围、单选配置。

```cpp
#include <oneui/controls/radio_group.h>
RadioGroup()
```

| API | 说明 |
|---|---|
| `setItems(std::vector<std::wstring>)` | 设置选项文本。 |
| `setSelectedIndex(int) / selectedIndex()` | 设置或读取选中项。 |
| `setOrientation(Orientation) / orientation()` | 设置 Vertical 或 Horizontal。 |
| `bindSelectedIndex(State<int>&)` | 绑定选中索引。 |
| `setOnChanged(std::function<void(int)>)` | 变化回调。 |
| `setStyleOverride(RadioGroupStyleOverride)` | 覆盖普通、悬停、按下、禁用、选中和焦点样式。 |

```cpp
auto group = std::make_shared<oneui::RadioGroup>();
group->setItems({L"低", L"中", L"高"});
group->setOrientation(oneui::RadioGroup::Orientation::Horizontal);
```

### Tabs

在多个同级视图之间切换，例如概览、日志、设置。

```cpp
#include <oneui/controls/tabs.h>
Tabs()
```

| API | 说明 |
|---|---|
| `setItems(std::vector<std::wstring>)` | 设置标签项。 |
| `setSelectedIndex(int) / selectedIndex()` | 设置或读取当前标签。 |
| `bindSelectedIndex(State<int>&)` | 绑定选中索引。 |
| `setOnChanged(std::function<void(int)>)` | 切换回调。 |
| `setStyleOverride(TabsStyleOverride)` | 覆盖普通、悬停、按下、禁用、选中和焦点样式。 |

```cpp
auto tabs = std::make_shared<oneui::Tabs>();
tabs->setItems({L"概览", L"连接", L"日志"});
```

## 数据展示与产品控件

这些组件用于列表、表格、导航、状态条、通知和窗口标题栏。

### List

展示可选择的项目列表，每项包含 title 和 detail。

```cpp
#include <oneui/controls/list.h>
List()
```

| API | 说明 |
|---|---|
| `setItems(std::vector<ListItem>)` | 设置列表项，ListItem 包含 title、detail。 |
| `setSelectedIndex(int) / selectedIndex()` | 设置或读取选中行。 |
| `bindSelectedIndex(State<int>&)` | 绑定选中索引。 |
| `setOnChanged(std::function<void(int)>)` | 选中行变化回调。 |
| `setStyleOverride(ListStyleOverride)` | 覆盖行背景、文本、边框、焦点等样式。 |

```cpp
oneui::ListItem item{L"主机 A", L"192.168.1.10"};
auto list = std::make_shared<oneui::List>();
list->setItems({item});
```

### Table

展示二维文本数据，适合简单报表、设备清单、连接列表。

```cpp
#include <oneui/controls/table.h>
Table()
```

| API | 说明 |
|---|---|
| `setColumns(std::vector<TableColumn>)` | 设置列头和列宽。 |
| `setRows(std::vector<std::vector<std::wstring>>)` | 设置表格行数据。 |
| `setStyleOverride(TableStyleOverride)` | 覆盖表头、行、边框、文本等样式。 |
| `clearStyleOverride()` | 恢复默认样式。 |

```cpp
auto table = std::make_shared<oneui::Table>();
table->setColumns({{L"名称", 180}, {L"状态", 100}});
table->setRows({{L"节点 A", L"在线"}});
```

### NavItem

侧边栏导航项，包含图标、文本、选中状态和点击行为。

```cpp
#include <oneui/controls/nav_item.h>
NavItem(std::wstring text, IconSymbol symbol, bool selected = false)
```

| API | 说明 |
|---|---|
| `setText(std::wstring)` | 设置导航文本。 |
| `setSymbol(IconSymbol)` | 设置导航图标。 |
| `setSelected(bool) / selected()` | 设置或读取选中状态。 |
| `setOnClick(std::function<void()>)` | 点击回调。 |
| `setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)` | 应用 CSS-like 样式。 |

```cpp
auto home = std::make_shared<oneui::NavItem>(L"首页", oneui::IconSymbol::Home, true);
```

### Tile

可点击的信息入口，包含标题、副标题、前导图标和尾随图标。

```cpp
#include <oneui/controls/tile.h>
Tile(std::wstring title = L"", std::wstring subtitle = L"")
```

| API | 说明 |
|---|---|
| `setTitle(std::wstring)` | 设置主标题。 |
| `setSubtitle(std::wstring)` | 设置副标题。 |
| `setLeadingSymbol(IconSymbol) / clearLeadingSymbol()` | 设置或清除左侧图标。 |
| `setTrailingSymbol(IconSymbol) / clearTrailingSymbol()` | 设置或清除右侧图标。 |
| `setOnClick(std::function<void()>)` | 点击回调。 |
| `setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)` | 应用 CSS-like 样式。 |

```cpp
auto tile = std::make_shared<oneui::Tile>(L"远程协助", L"连接到一台设备");
tile->setLeadingSymbol(oneui::IconSymbol::RemoteAssist);
```

### StatusStrip

页面底部或面板底部的状态条，可带主/次操作。

```cpp
#include <oneui/controls/status_strip.h>
StatusStrip(std::wstring title = L"", std::wstring message = L"")
```

| API | 说明 |
|---|---|
| `setTitle(std::wstring)` | 设置状态标题。 |
| `setMessage(std::wstring)` | 设置状态说明。 |
| `setPrimaryAction(std::wstring)` | 设置主操作按钮文本。 |
| `setSecondaryAction(std::wstring)` | 设置次操作按钮文本。 |
| `setOnPrimaryAction(std::function<void()>)` | 主操作回调。 |
| `setOnSecondaryAction(std::function<void()>)` | 次操作回调。 |
| `setPrimaryActionPresentation / setPrimaryActionTrailingIcon` | 主操作可以链接形式呈现，并附 trailing 图标。 |
| `setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)` | 应用 CSS-like 样式。 |

```cpp
auto strip = std::make_shared<oneui::StatusStrip>(L"已连接", L"延迟 24ms");
strip->setPrimaryAction(L"断开");
```

### Toast

短暂通知卡片，可带图标、主/次操作和关闭按钮。

```cpp
#include <oneui/controls/toast.h>
Toast(std::wstring title = L"", std::wstring message = L"")
```

| API | 说明 |
|---|---|
| `setTitle(std::wstring)` | 设置通知标题。 |
| `setMessage(std::wstring)` | 设置通知正文。 |
| `setPrimaryAction(std::wstring)` | 设置主操作文本。 |
| `setSecondaryAction(std::wstring)` | 设置次操作文本。 |
| `setIconSymbol(IconSymbol) / clearIconSymbol()` | 设置或清除图标。 |
| `setCloseVisible(bool)` | 控制关闭按钮可见性。 |
| `setOnPrimaryAction / setOnSecondaryAction / setOnClose` | 设置操作和关闭回调。 |
| `setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)` | 应用 CSS-like 样式。 |

```cpp
auto toast = std::make_shared<oneui::Toast>(L"保存成功", L"配置已写入本机");
toast->setCloseVisible(true);
```

### WindowTitleBar

自绘窗口标题栏，支持 borderless 窗口；Wayland 标准窗口也复用此组件绘制客户端装饰。

```cpp
#include <oneui/controls/window_title_bar.h>
WindowTitleBar(std::wstring title = L"")
```

| API | 说明 |
|---|---|
| `setTitle(std::wstring)` | 设置标题。 |
| `setIconSymbol(IconSymbol)` | 设置窗口图标。 |
| `setMaximized(bool)` | 设置最大化视觉状态。 |
| `setLeading(widget) / setAccessory(widget)` | 在标题栏挂载 leading 和交互附件；拖拽边界由窗口管理。 |
| `setOnMinimize / setOnMaximize / setOnClose` | 设置窗口控制按钮回调。 |
| `setStyleSheet(std::shared_ptr<StyleSheet>, StyleNode)` | 应用标题栏样式。 |

```cpp
auto titleBar = std::make_shared<oneui::WindowTitleBar>(L"OneUI 控制台");
titleBar->setOnClose([window] { window->close(); });
```

## 表单、弹层与远程场景

这些组件用于表单组织、验证提示、弹层挂载，以及远程桌面/远程控制相关能力。

### FormField

给输入控件加 label、helper、error、required、invalid 状态和可访问性关联。

```cpp
#include <oneui/controls/form_field.h>
FormField()
```

| API | 说明 |
|---|---|
| `setChild(std::shared_ptr<Widget>) / child()` | 设置或读取内部输入控件。 |
| `clearChild()` | 移除子控件。 |
| `setLabel / label / bindLabel` | 设置、读取或绑定标签。 |
| `setHelperText / helperText / bindHelperText` | 设置、读取或绑定帮助文本。 |
| `setErrorText / errorText / bindErrorText` | 设置、读取或绑定错误文本。 |
| `setRequired / required / bindRequired` | 设置、读取或绑定必填状态。 |
| `setInvalid / invalid / bindInvalid` | 设置、读取或绑定错误状态。 |
| `setStyleOverride(FormFieldStyleOverride)` | 覆盖 label、helper、error、间距等样式。 |

```cpp
auto field = std::make_shared<oneui::FormField>();
field->setLabel(L"连接地址");
field->setRequired(true);
field->setChild(addressInput);
```

### ValidationMessage

单独显示帮助或错误信息。

```cpp
#include <oneui/controls/validation_message.h>
ValidationMessage(std::wstring text = L"")
```

| API | 说明 |
|---|---|
| `setText(std::wstring) / text()` | 设置或读取提示文本。 |
| `bindText(State<std::wstring>&)` | 绑定提示文本。 |
| `setTone(ValidationMessageTone) / tone()` | 设置 Helper 或 Error。 |
| `setStyleOverride(ValidationMessageStyleOverride)` | 覆盖颜色、图标、字体等。 |

```cpp
auto error = std::make_shared<oneui::ValidationMessage>(L"地址不能为空");
error->setTone(oneui::ValidationMessageTone::Error);
```

### Popup

锚定到某个区域的弹层，支持首选位置、翻转、轻关闭、模态和外部指针策略。

```cpp
#include <oneui/controls/popup.h>
Popup()
```

| API | 说明 |
|---|---|
| `setAnchor(std::shared_ptr<Widget>) / anchor()` | 设置锚点控件。 |
| `setContent(std::shared_ptr<Widget>) / content()` | 设置弹层内容。 |
| `setOpen(bool) / isOpen() / bindOpen(State<bool>&)` | 控制打开状态。 |
| `setPreferredPlacement(PopupPreferredPlacement)` | 设置 BottomStart、TopEnd 等首选位置。 |
| `setViewport(std::optional<Rect>) / clearViewport()` | 设置可用视口。 |
| `setAnchorRect(std::optional<Rect>) / clearAnchorRect()` | 显式设置锚点矩形。 |
| `setCloseOnOutsideClick(bool)` | 外部点击时关闭。 |
| `setInteractionMode(PopupInteractionMode)` | 设置 Modeless、LightDismiss、Modal。 |
| `overlayOptions(int layer = 0)` | 生成 OverlayHost 可用的挂载选项。 |
| `setOutsidePointerPolicy(PopupOutsidePointerPolicy)` | 设置 PassThrough、Close 或 Block。 |
| `setCloseOnEscape(bool)` | Esc 关闭。 |
| `resolvedContentRect()` | 读取最终内容矩形。 |

```cpp
auto popup = std::make_shared<oneui::Popup>();
popup->setAnchor(button);
popup->setContent(menu);
popup->setPreferredPlacement(oneui::PopupPreferredPlacement::BottomStart);
popup->setOpen(true);
```

### OverlayHost

在普通内容之上挂载弹层、通知、菜单、模态层。

```cpp
#include <oneui/layout/overlay_host.h>
OverlayHost()
```

| API | 说明 |
|---|---|
| `setContent(std::shared_ptr<Widget>)` | 设置底层主内容。 |
| `addOverlay(child, int layer)` | 按层级添加普通 overlay。 |
| `addOverlay(child, OverlayOptions)` | 使用 trapsFocus、blocksOutsidePointer 等选项添加 overlay。 |
| `addAnchoredOverlay(child, options, Size, Insets, hAlign, vAlign)` | 添加带锚定尺寸和对齐的 overlay。 |
| `removeOverlay(std::shared_ptr<Widget>)` | 移除指定 overlay。 |
| `clearOverlays()` | 清空所有 overlay。 |
| `overlays()` | 读取 overlay 列表。 |

```cpp
auto host = std::make_shared<oneui::OverlayHost>();
host->setContent(app);
host->addOverlay(toast, oneui::OverlayOptions::modeless(10));
```

### RealtimeFrameView

显示远程画面最新帧，并按 ActualSize、Fit、Fill、Stretch 计算内容矩形。

```cpp
#include <oneui/controls/realtime_frame_view.h>
RealtimeFrameView()
```

| API | 说明 |
|---|---|
| `submitFrame(VideoFrame)` | 提交 BGRA/RGBA 帧；NV12 仅保留枚举，未实现转换。 |
| `setScaleMode(ScaleMode) / scaleMode()` | 设置或读取缩放模式。 |
| `contentRect()` | 读取当前帧在控件中的实际显示矩形。 |
| `latestFrame()` | 读取最新帧快照。 |

```cpp
oneui::VideoFrame frame{};
frame.width = width;
frame.height = height;
frame.format = oneui::PixelFormat::Bgra8888;
view->submitFrame(frame);
```

### RemoteInputRegion

把本地鼠标和键盘输入映射成远端坐标与原始按键事件。

```cpp
#include <oneui/controls/remote_input_region.h>
RemoteInputRegion()
```

| API | 说明 |
|---|---|
| `setRemoteSize(Size) / remoteSize()` | 设置远端画面尺寸。 |
| `setScaleMode(RemoteInputScaleMode) / scaleMode()` | 设置坐标映射缩放模式。 |
| `contentRect()` | 读取远端画面在本地控件中的内容矩形。 |
| `setOnPointer(PointerCallback)` | 设置指针事件回调，包含本地、内容、归一化、远端坐标。 |
| `setOnRawKey(RawKeyCallback)` | 设置原始键盘事件回调；Control 和 Meta 保留物理含义。 |
| `setOnTextInput(TextInputCallback)` | 接收 IME/Unicode 提交文本，与可打印原始按键去重。 |
| `setRemoteCursorMode / setRemoteCursorBitmap` | 默认、隐藏或位图远程光标；支持线程安全更新句柄。 |
| `dispatchPointer(RemotePointerEvent)` | 手动派发指针事件。 |
| `dispatchRawKey(RawKeyEvent)` | 手动派发键盘事件。 |
| `releaseAllInputs()` | 释放所有已按下输入，适合连接断开或焦点丢失。 |

```cpp
region->setRemoteSize(oneui::Size{1920, 1080});
region->setOnPointer([](const oneui::RemotePointerEvent& e) {
  sendPointer(e.remotePosition, e.button, e.pressed);
});
```

## 数据、终端与工作区原语

这些控件复用共享 Canvas 和输入路由，并具有 C ABI 与安全 Rust 入口；平台服务的可用性需独立查询。

### VirtualList

固定行高的虚拟列表；普通 ListItem 与富 VirtualListItem 分离。

```cpp
#include <oneui/controls/virtual_list.h>
VirtualList()
```

| API | 说明 |
|---|---|
| `setRichItems / updateRichItem` | 批量与原位更新 title、detail、badge、trailing、状态指示和颜色。 |
| `setRichMetrics / richMetrics` | 配置和读取富行指标；窄行压缩或裁剪，不覆盖文字。 |
| `setScrollOffset / setSelectedIndices` | 管理滚动及选择；worker handle 合并后台数据更新。 |

```cpp
auto list = std::make_shared<oneui::VirtualList>();
```

### TreeView

使用稳定 ID 的层级树。

```cpp
#include <oneui/controls/tree_view.h>
TreeView()
```

| API | 说明 |
|---|---|
| `setItems / setSelectedId` | 提交层级数据、按 ID 选择。 |
| `setExpanded / setOnExpansionChanged` | 展开与变化回调。 |
| `setOnReorderRequested` | 报告重排请求，由产品更新数据。 |

```cpp
auto tree = std::make_shared<oneui::TreeView>();
```

### TerminalView

终端网格、宽字符单元格、光标、选区与输入回调。

```cpp
#include <oneui/controls/terminal_view.h>
TerminalView()
```

| API | 说明 |
|---|---|
| `setGrid / setCursor` | 更新单元格和光标。 |
| `textInputCaretRect` | 提供输入法候选位置。 |
| `setOnTextInput` | 接收完整 Unicode 提交文本。 |

```cpp
auto terminal = std::make_shared<oneui::TerminalView>();
```

### LogView

带逐行颜色和选择复制的只读日志。

```cpp
#include <oneui/controls/log_view.h>
LogView()
```

| API | 说明 |
|---|---|
| `appendLine / clearLines` | 追加和清空日志。 |
| `contentHeight` | 供 ScrollView 计算内容高度。 |
| `selectAll / selectedText` | 读取选区文本。 |

```cpp
auto log = std::make_shared<oneui::LogView>();
```

### TimeSeriesChart

带空洞、阈值、曲线与 inspection 的时间序列图。

```cpp
#include <oneui/controls/time_series_chart.h>
TimeSeriesChart()
```

| API | 说明 |
|---|---|
| `setSeries / setRange` | 设置序列与范围。 |
| `setThresholds` | 配置阈值线。 |
| `setInspectionIndex / setOnInspectionChanged` | 查看数据点与固定检查状态。 |

```cpp
auto chart = std::make_shared<oneui::TimeSeriesChart>();
```

## 桥接与计算型组件

这些不是普通页面控件，更多用于把 OneUI 的视觉/布局计算复用到原生平台控件或产品壳实现里。

### TextInputBridge

计算自绘输入框与原生编辑器之间的位置、显示和预留区域。

```cpp
#include <oneui/controls/text_input_bridge.h>
纯函数和结构体，无需构造控件
```

| API | 说明 |
|---|---|
| `TextInputBridgeConfig` | 输入 frame、focused、hovered、disabled、readOnly、revealNativeEditor、leadingReservedWidth 等配置。 |
| `TextInputBridgeLayout` | 输出 frame、contentRect、editorRect、hiddenEditorRect、showNativeEditor。 |
| `textInputBridgeState(config)` | 根据状态得到桥接状态。 |
| `computeTextInputBridgeLayout(config)` | 计算编辑器矩形。 |

```cpp
auto layout = oneui::computeTextInputBridgeLayout(config);
```

### ButtonBridge

计算按钮桥接状态、布局和命中测试。

```cpp
#include <oneui/controls/button_bridge.h>
纯函数和结构体，无需构造控件
```

| API | 说明 |
|---|---|
| `buttonBridgeState(config)` | 根据 hovered、pressed、disabled 等得到按钮状态。 |
| `computeButtonBridgeLayout(config)` | 计算图文区域。 |
| `hitTestButtonBridge(layout, point)` | 命中测试。 |

```cpp
if (oneui::hitTestButtonBridge(layout, mouse)) { /* hover */ }
```

### ProductShell Helpers

为产品级布局计算标准矩形，例如侧边栏、顶栏、仪表盘、表单行、状态条、窗口 chrome。

```cpp
#include <oneui/layout/product_shell.h>
ProductShell 是控件；computeProduct* 是纯计算函数
```

| API | 说明 |
|---|---|
| `computeProductShellLayout(...)` | 计算产品壳主体区域。 |
| `computeProductSidebarLayout(...)` | 计算侧边栏区域。 |
| `computeProductTopBarLayout(...)` | 计算顶栏区域。 |
| `computeProductDashboardLayout(...)` | 计算仪表盘卡片区域。 |
| `computeProductFormRowLayout(...)` | 计算表单行区域。 |
| `computeProductAssistHomeLayout(...)` | 计算远程协助首页区域。 |
| `computeProductStatusStripLayout(...)` | 计算状态条区域。 |
| `computeProductWindowChromeLayout(...)` | 计算窗口 chrome 区域。 |

```cpp
auto layout = oneui::computeProductDashboardLayout(metrics);
```

### Animation / StyleTransition

提供 FloatTransition、ColorTransition、StyleBoxTransition 和 easing 计算。

```cpp
#include <oneui/animation.h> 与 #include <oneui/style_transition.h>
FloatTransition(), ColorTransition(), StyleBoxTransition()
```

| API | 说明 |
|---|---|
| `TransitionSpec{durationMs, easing}` | 描述动画时长与 easing。 |
| `applyEasing(EasingCurve, float)` | 计算 easing 后的进度。 |
| `interpolateFloat / interpolateColor` | 插值工具。 |
| `reset(value)` | 立即重置当前值和目标值。 |
| `animateTo(target, spec)` | 启动过渡。 |
| `tick(deltaMs)` | 推进动画，返回是否仍在运行。 |
| `value() / target() / running()` | 读取当前值、目标值和运行状态。 |
| `StyleBoxTransition::applyTo(StyleBox&)` | 把过渡值写入样式盒。 |

```cpp
oneui::FloatTransition opacity;
opacity.reset(0.0f);
opacity.animateTo(1.0f, oneui::TransitionSpec{160, oneui::EasingCurve::EaseOutCubic});
```
