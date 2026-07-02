# OneUI 可访问性架构草案

这份文档描述 OneUI 的可访问性方向。当前运行时已经有 `Widget` 基础语义 API，可以声明 role、name、description、value 和部分 state；`Button`、`TextField`、`Checkbox`、`Select`、`Slider`、`RadioGroup`、`Tabs`、`List`、`Table` 已经开始提供默认语义，`FormField` 会把 label/helper/error/required/invalid 同步到子控件。但还没有完整平台 accessibility bridge。本文不代表 Windows UI Automation、macOS Accessibility 或 Linux AT-SPI 已经实现。

## 目标

OneUI 的可访问性要服务三个对象：

- 键盘用户：所有可操作控件都能通过键盘到达和操作。
- 辅助技术用户：屏幕阅读器能读出控件角色、名称、值、状态和错误说明。
- 应用开发者：可以用接近 HTML/Vue3 的方式给控件声明语义，而不是把平台 API 散落在业务代码里。

## 分层

```text
Widget / View
  -> AccessibilityNode: role、name、description、value、state
  -> AccessibilityTree: 从 widget tree 派生语义树
  -> PlatformBridge: UI Automation / AX / AT-SPI
```

### Widget 语义

每个 `Widget` 已经可以暴露一个轻量语义对象：

```cpp
struct AccessibilityInfo {
    AccessibilityRole role;
    std::wstring name;
    std::wstring description;
    std::wstring value;
    AccessibilityState state;
};
```

控件默认语义应由控件类型给出：

| 控件 | 默认 role | name 来源 | value 来源 |
| --- | --- | --- | --- |
| `Button` | Button | 按钮文本，显式 accessible name 优先 | 空 |
| `TextField` | TextBox | 显式 accessible name / placeholder | 当前文本 |
| `Select` | ComboBox | 显式 accessible name | 当前选项文本 |
| `Checkbox` | CheckBox | 显式 accessible name / label | checked / unchecked |
| `RadioGroup` | RadioGroup | label | 当前选项 |
| `Tabs` | TabList + Tab | tab 文案 | selected tab |
| `Slider` | Slider | 显式 accessible name | 当前数值 |
| `List` | List | 显式 accessible name | 当前选中项 |
| `Table` | Table | 显式 accessible name | 行列数量 |
| `Popup` | Popup / Menu | 触发器或内容语义 | open / closed |

### FormField 关联

`FormField` 是可访问性的重要入口，已经会把 label、helper、error、required 和 invalid 状态关联到子控件：

```cpp
auto field = std::make_shared<oneui::FormField>();
field->setLabel(L"项目名称");
field->setRequired(true);
field->setErrorText(L"项目名称不能为空");
field->setChild(input);
```

当前语义规则：

- 子控件的 accessible name 优先来自开发者显式设置；如果子控件没有显式 name，则来自 `FormField::label`。
- `required` 映射为 required state。
- `invalid` 映射为 invalid state。
- `helperText` / `errorText` 映射为 description。
- 错误优先级高于 helper。
- 如果开发者已经在子控件上显式设置 accessible name/description，`FormField` 不会继续覆盖它。

## 状态模型

建议从这些状态开始：

```cpp
struct AccessibilityState {
    bool disabled = false;
    bool focused = false;
    bool focusVisible = false;
    bool selected = false;
    bool checked = false;
    bool pressed = false;
    bool expanded = false;
    bool required = false;
    bool invalid = false;
    bool readOnly = false;
};
```

状态来源应该复用现有控件状态，避免出现“视觉 disabled、语义 enabled”这类分裂。

## 键盘规则

现阶段已经有基础焦点模型：`View`、`OverlayHost`、`TextField`、`Select` 等能处理一部分键盘事件。后续需要补齐：

- `Tab` / `Shift+Tab`：在可聚焦控件之间移动。
- `Enter` / `Space`：触发按钮、复选框、选项提交。
- `Escape`：关闭最上层 Popup / Select / Dialog。
- `Home` / `End` / `Delete`：文本编辑和列表导航按场景处理。
- roving focus：菜单、列表、tab、radio group 内部用方向键移动焦点或选中项。

键盘行为应由控件自己处理，平台后端只负责把平台按键映射成 `KeyEvent`。

## 平台桥接

后续平台层应分别实现：

- Windows：UI Automation provider。
- macOS：NSAccessibility。
- Linux：AT-SPI。

平台桥接不应直接读取业务状态。它应该读取 OneUI 语义树，这样跨平台行为才能一致。

## API 方向

推荐公开 API 先保持简单：

```cpp
button->setAccessibleName(L"保存项目");
input->setAccessibleDescription(L"请输入 2 到 32 个字符");
input->setAccessibleRole(oneui::AccessibilityRole::TextBox);

auto info = input->accessibilityInfo();
// info.state.disabled / focused / focusVisible 会跟随运行时状态更新
```

但控件默认语义更重要。大多数应用开发者不应该每个控件都手写 role；只有图标按钮、自定义绘制控件、复杂组合控件需要显式覆盖。

## 验收清单

第一阶段可访问性 MVP 应满足：

- Button、TextField、Select、Checkbox、RadioGroup、Tabs 有默认 role/name/value/state。
- FormField 能把 label/helper/error/required/invalid 关联到子控件。
- 键盘焦点顺序和视觉焦点环一致。
- Popup/OverlayHost 的焦点进入、退出和 Escape 关闭规则可测试。
- Windows 后端能被基础 UI Automation 工具读出 name、role、value。
- Gallery 有一个“可访问性”页面，展示表单、错误、弹层和键盘焦点。

## 当前状态

| 能力 | 状态 | 说明 |
| --- | --- | --- |
| 键盘焦点基础 | 部分完成 | `View`、`OverlayHost`、部分控件已有基础焦点行为 |
| TextField 基础编辑键 | 部分完成 | 已有 caret、基础选择区、copy/cut/paste API、Win32 系统剪贴板 bridge、Ctrl+A/C/X/V、Left/Right/Home/End/Delete/Backspace |
| 语义 role/name/value | 部分完成 | `Widget` 已有基础公开 API；常用表单、选择和数据展示控件已有默认语义；平台语义树待做 |
| FormField 语义关联 | 部分完成 | label/helper/error/required/invalid 已同步到子控件；更复杂的 described-by 语义树待做 |
| 平台 accessibility bridge | 未完成 | Windows/macOS/Linux 都未实现 |
| 自动化测试 | 部分完成 | 已覆盖 `Widget::accessibilityInfo()` 基础语义、动态状态，以及 Button/TextField/Checkbox/Select/Slider/RadioGroup/Tabs/List/Table 默认语义 |

## 不做的事

- 不把 Web DOM 或 ARIA 直接搬进运行时。
- 不让应用层直接操作 UI Automation / AX / AT-SPI 对象。
- 不在语义未完成时宣称屏幕阅读器完整支持。
- 不牺牲 Win7 和零终端依赖目标。
