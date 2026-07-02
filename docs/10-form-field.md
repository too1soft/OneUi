# FormField 与 ValidationMessage

本文记录 `FormField` 和 `ValidationMessage` 已接受的 runtime slice。

## FormField

`FormField` 包装一个子控件，并在其周围绘制 label、required marker、helper text 或 error text。它同时会把这些表单语义同步到子控件，供 `accessibilityInfo()` 使用。

已接受 API：

- `setChild(std::shared_ptr<Widget> child)`：安装被包装的子控件。
- `setLabel(std::wstring label)` / `bindLabel(State<std::wstring>& state)`：设置或绑定 label。
- `setHelperText(std::wstring text)` / `bindHelperText(State<std::wstring>& state)`：设置或绑定 helper text。
- `setErrorText(std::wstring text)` / `bindErrorText(State<std::wstring>& state)`：设置或绑定 error text。
- `setRequired(bool required)` / `bindRequired(State<bool>& state)`：设置或绑定 required marker。
- `setInvalid(bool invalid)` / `bindInvalid(State<bool>& state)`：设置或绑定 validation state。
- `setStyleOverride(FormFieldStyleOverride style)`：应用控件级样式覆盖。
- `clearStyleOverride()`：清除覆盖并回到主题默认值。

行为：

- `invalid == true` 时，`errorText` 是当前消息，并优先于 `helperText`。
- `invalid == false` 时，如果存在 `helperText` 就显示 helper text。
- `required == true` 时，在 label 旁绘制 required marker。
- 子控件通过 wrapper 接收 pointer、wheel、keyboard、text-input 和 focus 交互。
- 子控件没有显式 accessible name 时，`FormField` 会用 label 作为子控件 name。
- 子控件没有显式 accessible description 时，`FormField` 会用 helper/error 作为子控件 description，且 error 优先于 helper。
- `required` 和 `invalid` 会同步到子控件 accessibility state。

样式覆盖字段：

- `labelColor`
- `helperColor`
- `errorColor`
- `requiredMarkerColor`
- `padding`
- `labelFontSize`
- `messageFontSize`
- `labelLineHeight`
- `messageLineHeight`
- `labelGap`
- `controlGap`

示例：

```cpp
oneui::State<std::wstring> label{L"Project key"};
oneui::State<bool> invalid{true};

auto input = std::make_shared<oneui::TextField>(L"one-ui");
auto field = std::make_shared<oneui::FormField>();
field->bindLabel(label);
field->setHelperText(L"Shown when the field is valid.");
field->setErrorText(L"Use lowercase letters and hyphens.");
field->setRequired(true);
field->bindInvalid(invalid);
field->setChild(input);

oneui::FormFieldStyleOverride style;
style.labelFontSize = 12.0f;
style.messageFontSize = 11.0f;
style.controlGap = 4.0f;
field->setStyleOverride(style);
```

## ValidationMessage

`ValidationMessage` 用表单消息 token 绘制独立的 helper/error 文本。

已接受 API：

- `setText(std::wstring text)` / `bindText(State<std::wstring>& state)`：设置或绑定文本。
- `setTone(ValidationMessageTone tone)`：选择 `Helper` 或 `Error`。
- `setStyleOverride(ValidationMessageStyleOverride style)`：应用控件级样式覆盖。
- `clearStyleOverride()`：清除覆盖并回到主题默认值。

样式覆盖字段：

- `helperColor`
- `errorColor`
- `fontSize`
- `lineHeight`

示例：

```cpp
auto message = std::make_shared<oneui::ValidationMessage>(
    L"ValidationMessage uses form tokens.");
message->setTone(oneui::ValidationMessageTone::Helper);

oneui::ValidationMessageStyleOverride compact;
compact.fontSize = 11.0f;
compact.lineHeight = 14.0f;
message->setStyleOverride(compact);
```

## Gallery 使用

构建所选工具链后运行 Gallery：

```powershell
.\scripts\run-gallery.ps1
```

在 `Controls` 区域检查项目名称、项目 key、平台和实时预览的 `FormField` 包装示例。

项目 key 示例故意处于 invalid 状态，用于证明 `invalid == true` 时 error text 会覆盖 helper text。

在 `Style` 区域检查主题默认值、状态样本和组件级 style override 行为。
