# OneUI 样式方向

## 样式 MVP

OneUI 当前采用类型化的 CSS-like 样式方向。它不是运行时 CSS parser，也没有字符串 cascade；现阶段的目标是先用 C++ 类型把主题 token、控件状态和局部 override 固定下来。

已接受的基础概念：

- `ControlState`：共享控件状态词汇，包括 default、hovered、pressed、disabled、focus-visible、selected。
- `FocusRingStyle` / `FocusRingStyleOverride`：键盘焦点外框样式。
- `BoxShadow` / `Canvas::drawBoxShadow(...)`：Canvas 级阴影绘制原语，供控件复用。
- `ButtonStyleOverride`、`TextFieldStyleOverride`、`SliderStyleOverride`、`ProgressBarStyleOverride`、`SelectStyleOverride`、`CheckboxStyleOverride`、`RadioGroupStyleOverride`、`TabsStyleOverride`、`BadgeStyleOverride`、`SeparatorStyleOverride`、`ListStyleOverride`、`TableStyleOverride`、`FormFieldStyleOverride`、`ValidationMessageStyleOverride`、`PopupStyleOverride`：局部样式覆盖。
- override 字段使用 `std::optional`，只替换明确传入的值。

解析顺序：

```text
component default -> normal override -> active state override -> focusVisible override
```

## 示例

```cpp
oneui::ButtonStyleOverride buttonStyle;
oneui::ButtonStateStyleOverride normal;
normal.background = oneui::Color{12, 34, 56};
normal.foreground = oneui::Color{255, 255, 255};
buttonStyle.normal = normal;

oneui::Button button(L"Save");
button.setStyleOverride(buttonStyle);
```

## 视觉方向

OneUI 是桌面软件 UI，不是营销页。默认视觉应安静、精确、实用。

默认风格：

- 中性表面。
- 清晰边框。
- 小圆角。
- 阴影只用于表达层级。
- 动效克制、快速。
- 交互态、禁用态、选中态和焦点态清楚可辨。

## Canvas 阴影原语

OneUI 已接受 Canvas 级 BoxShadow API，用于把控件里的阴影绘制收敛成可复用原语：

```cpp
oneui::BoxShadow shadow;
shadow.color = oneui::Color{17, 24, 39, 40};
shadow.offset = oneui::Point{0.0f, 6.0f};
shadow.blurRadius = 18.0f;
shadow.spreadRadius = 0.0f;

canvas.drawBoxShadow(rect, shadow, 8.0f);
```

字段含义：

- `color`：阴影颜色，通常使用带透明度的中性色。
- `offset`：阴影相对目标矩形的偏移。
- `blurRadius`：模糊半径。
- `spreadRadius`：扩张或收缩阴影外形。
- `radius` 参数：和目标矩形相同的圆角半径。

控件需要表达层级时应优先复用 `drawBoxShadow`，再绘制自身 surface 和边框。这个 API 现在是渲染基础设施，不代表所有控件默认已经应用阴影；`Card` 和 `Popup` 已接入基础阴影，Dialog、Menu、Tooltip 等仍需要后续迭代逐步接入。没有原生阴影能力的后端可以保守地保持 no-op。

## 初始 Token

> 说明：下表是最早期的设计取值，仅作方向示意。**运行时权威主题以 `include/oneui/style.h` 的 `Theme` 结构为准**，例如现值 `surface #ffffff`、`surfaceMuted #f8fafc`、`appBackground #f2f4f7`、`text #191c21`、`border #d3d8e0`（`primary #2563eb`、`primaryHover #1d4ed8` 与下表一致）。改主题请改 `Theme`，勿以本表为源。

```text
color.surface       #f7f7f8   （现为 appBackground/surfaceMuted，见 style.h）
color.panel         #ffffff   （现为 Theme.surface）
color.text          #202124   （现 #191c21）
color.text-muted    #666a70   （现 #616772）
color.border        #d8dbe0   （现 #d3d8e0）
color.primary       #2563eb
color.primary-hover #1d4ed8
color.danger        #dc2626

radius.sm           4
radius.md           6

spacing.2           2
spacing.4           4
spacing.8           8
spacing.12          12
spacing.16          16
spacing.24          24
```

## 状态 Token 矩阵

主题状态 token 将常见交互状态拆成背景、前景和边框。控件应复用这些语义字段，避免在每个控件里临时造一套相似颜色。

| 状态 | 背景字段 | 前景字段 | 边框字段 |
| --- | --- | --- | --- |
| disabled | `disabledBackground` | `disabledForeground` | `disabledBorder` |
| hover | `hoverBackground` | `hoverForeground` | `hoverBorder` |
| pressed | `pressedBackground` | `pressedForeground` | `pressedBorder` |
| selected | `selectedBackground` | `selectedForeground` | `selectedBorder` |

## 控件规则

第一个 Button 应该是“无聊但可靠”的控件：

- 矩形清晰。
- 6px 圆角。
- 尺寸稳定。
- hover 和 pressed 状态明确。
- 文本水平、垂直居中。
- 不使用模糊、装饰渐变或平台专属特效。

同样原则适用于后续控件：状态清楚，样式可覆盖，默认值克制。
