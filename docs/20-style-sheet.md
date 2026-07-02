# StyleSheet

`StyleSheet` is the first reusable CSS-like styling layer in OneUI. It is meant
for native apps that need consistent surfaces, inputs, buttons, cards, shadows,
and interactive states without hardcoding paint details in every product.

For non-text glyphs, OneUI also exposes `buildIconPrimitives()`. It returns
backend-neutral icon primitives such as lines, circles, rounded rectangles, and
polylines, preserving per-primitive alpha so native painters can render soft
watermarks, switches, radio indicators, copy/disclosure affordances, and
password-dot rows without shipping product-specific image assets.

The API intentionally starts with the subset needed by desktop product shells:

- class selectors such as `.input`, `.button.primary`, `.recent-card`
- pseudo states such as `:hover`, `:active`, `:focus`, `:disabled`, `:selected`, `:read-only`
- background color and gradient endpoints
- foreground color
- border color, width, and radius
- outline color, width, and offset for focus rings and ambient edge glow
- content box background, radius, and inset for native text/edit child regions
- opacity for soft chrome, watermarks, and dimmed primitive layers
- padding
- outer and inset shadows
- deterministic merging by selector specificity and rule order

Example:

```cpp
oneui::StyleSheet sheet;
std::string error;
sheet.addRulesFromCss(R"css(
.button {
    background: #20222a;
    border-color: #363a46;
    border-radius: 6px;
}

.button.primary:hover {
    background: #3471f6;
    outline-color: #1f54da44;
    outline-width: 2px;
    outline-offset: 1px;
    content-background-color: #17181e;
    content-inset: 10px 12px;
    content-radius: 4px;
    opacity: 0.92;
    box-shadow: 0px 8px 18px #0000005a;
}

.input:read-only {
    background: #20222a;
    color: #a8afbd;
}
)css", &error);
```

The same rules can also be assembled programmatically:

```cpp
oneui::StyleSheet sheet;

oneui::StyleRule button;
button.selector = ".button";
button.box.background.color = oneui::Color{32, 34, 42};
button.box.borderColor = oneui::Color{54, 58, 70};
button.box.radius = 6.0f;
sheet.addRule(button);

oneui::StyleRule primaryHover;
primaryHover.selector = ".button.primary:hover";
primaryHover.box.background.color = oneui::Color{52, 113, 246};
primaryHover.box.shadows.push_back(
    oneui::StyleShadow{oneui::Color{0, 0, 0, 90}, oneui::Point{0, 8}, 18, 0, false});
sheet.addRule(primaryHover);

auto style = sheet.resolve(oneui::StyleNode{
    "button",
    {"button", "primary"},
    oneui::StyleStateHover});
```

For native hosts that are still migrating from owner-drawn Win32 or another
backend, use the semantic StyleBox helpers instead of resolving CSS directly in
product code:

```cpp
auto buttonBox = oneui::buttonStyleBoxFromStyleSheet(
    sheet,
    oneui::StyleNode{"button", {"button", "primary"}, oneui::StyleStateNone},
    oneui::StyleStateHover);

auto inputBox = oneui::textFieldStyleBoxFromStyleSheet(
    sheet,
    oneui::StyleNode{"input", {"input"}, oneui::StyleStateNone},
    oneui::StyleStateFocus);
```

For built-in controls, use the adapter layer instead of manually translating
tokens in product code:

```cpp
oneui::StyleSheet sheet;
sheet.addRulesFromCss(remoteThemeCss, &error);

auto connect = std::make_shared<oneui::Button>(L"连接");
connect->setStyleOverride(oneui::buttonStyleOverrideFromStyleSheet(
    sheet,
    oneui::StyleNode{"button", {"button", "primary"}, oneui::StyleStateNone}));

auto deviceCode = std::make_shared<oneui::TextField>(L"请输入伙伴识别码");
deviceCode->setStyleOverride(oneui::textFieldStyleOverrideFromStyleSheet(
    sheet,
    oneui::StyleNode{"input", {"input"}, oneui::StyleStateNone}));

auto logCard = std::make_shared<oneui::Card>();
logCard->setStyleBox(oneui::cardStyleBoxFromStyleSheet(
    sheet,
    oneui::StyleNode{"section", {"card", "log-card"}, oneui::StyleStateNone}));

oneui::ProductShellStyle shellStyle = oneui::productShellStyleFromStyleSheet(sheet);
```

This adapter API is the boundary required by product shells: applications may
provide selectors and classes, but Button/TextField/Card/ProductShell state
mapping remains inside OneUI.

The remote native client uses this layer for its Win7-compatible shell so the
Sunlogin-style dark theme can evolve from reusable OneUI rules instead of
one-off GDI constants.

Supported CSS-like declarations currently include:

- selectors with `:hover`, `:active`, `:focus`, `:disabled`, `:selected` / `:checked`, and `:read-only` / `:readonly`
- `background: #rrggbb`
- `background: linear-gradient(90deg, #rrggbb, #rrggbb)` with angle captured for native painters
- `color`
- `placeholder-color`
- `caret-color`
- `border-color`
- `border-width`
- `border-radius`
- `outline-color`
- `outline-width`
- `outline-offset`
- `content-background-color`
- `content-inset`
- `content-radius`
- `opacity`
- `padding`
- `box-shadow`, including `inset`

This is not a complete browser CSS engine yet. The intended growth path is:

1. add design-token variables,
2. add font and layout properties,
3. add native backend painters for gradients, blur, and high-quality shadows,
4. support separate external `.oui.css` files and live reload for designers.
