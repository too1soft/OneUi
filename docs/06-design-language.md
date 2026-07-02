# OneUI 设计语言

## 作者体验方向

OneUI 是原生 C++ 桌面框架，但作者体验应让熟悉 HTML、Vue 3 和 MVVM 的开发者感到自然。

对照关系：

```text
Vue component      -> OneUI Widget subclass
template tree      -> View/Stack/Grid child tree
text interpolation -> Label::bindText(State<std::wstring>&)
ref/reactive state -> State<T>
v-model            -> bindText / bindChecked / bindValue
v-bind             -> bindDisabled and future bindXxx props
v-show             -> bindVisible(State<bool>&)
props              -> setText / setVariant / setDisabled / setPreferredSize
events             -> setOnClick / setOnChanged
CSS flex           -> Stack row/column, gap, padding, align
CSS grid           -> Grid columns, gaps, padding, auto rows
CSS overflow       -> ScrollView viewport and wheel scrolling
CSS tokens         -> Theme colors, radius, font sizes
CSS state styles   -> typed per-component style overrides
```

## 样式模型

近期样式模型是强类型 API，而不是字符串 CSS 解析。控件从主题默认值、状态默认值和可选控件 override 中解析视觉表现。

这样做的目标是：保持原生 C++ 的可预测性，同时保留接近 CSS 的自由度。

当前 MVP：

- `Button::setStyleOverride(ButtonStyleOverride)`
- `Checkbox::setStyleOverride(CheckboxStyleOverride)`
- `FormField::setStyleOverride(FormFieldStyleOverride)`
- `ValidationMessage::setStyleOverride(ValidationMessageStyleOverride)`
- `PopupStyleOverride` for popup tokens
- `FocusRingStyleOverride` for keyboard focus
- `ControlState` as shared state vocabulary

## MVVM 方向

控件不应该拥有应用业务数据。控件展示状态并发出用户意图。

推荐模式：

```cpp
struct LoginViewModel {
    oneui::State<std::wstring> account{L""};
    oneui::State<bool> remember{true};
};

auto account = std::make_shared<oneui::TextField>(L"Account");
account->bindText(vm.account);

auto remember = std::make_shared<oneui::Checkbox>(L"Remember me");
remember->bindChecked(vm.remember);
```

控件可以保存 hover、pressed、focus 等瞬态交互状态。持久业务状态应放在 ViewModel 中，并通过绑定流入控件。

当 `State<T>` 在控件外变化时，绑定控件通过 scoped `Binding<T>` 订阅触发自身 invalidation。

## 组件规则

- 面向用户的控件必须支持鼠标、键盘焦点和状态变化后的重绘。
- 控件优先暴露清晰 setter，避免用过长构造函数承载所有配置。
- 禁用控件不接受鼠标、文本或键盘输入，并从焦点遍历中跳过。
- 不可见控件不绘制、不接受输入、不参与焦点遍历，布局容器跳过它们。
- `View` 对已经禁用或不可见的子控件停止分发键盘、文本和 mouse-up 事件。
- 双向控件使用 `bindXxx(State<T>&)` 命名。
- `bindDisabled(State<bool>&)` 是共享单向 prop binding，对应 Vue `:disabled`。
- `bindVisible(State<bool>&)` 对应 Vue `v-show`，不从子树移除控件。
- `bindXxx` 必须订阅状态变化，并随控件生命周期释放订阅。
- 事件回调使用 `setOnXxx(...)` 命名。
- 容器通过 `std::shared_ptr<Widget>` 持有子控件。
- 布局优先使用 `Stack`、`Grid`、`ScrollView` 等容器，少用绝对坐标。
- setter 在影响视觉输出时应调用 `invalidate()`。
- 平台后端负责最终 repaint scheduling，控件只请求 invalidation。

## 当前核心组件

组件清单以 **`07-component-inventory.md`** 为权威单一来源（本文不再另列一份，避免两处清单各自漂移）。速览：布局有 `Stack/Grid/Wrap/DockView/SplitView/ScrollView/Panel`，控件有 `Card/Label/Button/IconButton/TextField/FormField/ValidationMessage/Switch/Checkbox/Slider/ProgressBar/RadioGroup/Select/Tabs/List/Table/Badge`，浮层有 `OverlayHost/Popup/Toast`，外壳有 `AppShell/ProductShell`。

## 近期缺口

- `TextField` 已有基础选择区、密码显示模式、横向滚动裁剪、剪贴板抽象、Win32 系统剪贴板 bridge 和 Ctrl+A/C/X/V；仍缺 IME 和 Linux/macOS 剪贴板 bridge。
- `Select` 仍需迁移到共享 `OverlayHost` / `Popup` 基础设施。
- Tooltip、Menu、Dialog 还需要完整交互行为（`Popup`、`Toast` 已落地）。
- 文本测量、复杂脚本 shaping、截断、换行和图文混排仍需完善。
- 可访问性 roles、names、values 和平台桥接尚未实现。
- Linux 和 macOS 后端仍是 skeleton，尚未实现。
