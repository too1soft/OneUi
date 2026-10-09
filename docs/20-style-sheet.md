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

### Background-only transparency

`background-opacity` and `content-background-opacity` accept a number from 0 to
1 or a percentage (values outside the range are clamped). They multiply the
resolved fill alpha **after** the state cascade, including every gradient stop.
They do not fade text, icons, selection ink, borders, focus rings or descendants.
The two properties are independent: for a select, the content fill controls its
popup; for a table it controls its header. The universal selector `*` has zero
specificity and can establish a material policy for all styled controls:

```css
* { background-opacity: 20%; content-background-opacity: 20%; }
button { background: #303033; }
button:hover { background: #404045; }
select { background: #303033; content-background: #303033;
         content-background-opacity: 96%; }
```

Declare the relevant background colors in the stylesheet; unspecified fills
still belong to the control's native theme. To make content fully transparent,
set both multipliers to zero. Existing RGBA alpha remains multiplicative.
Dynamic custom-property updates invalidate the resolved cache. Switching the
material stylesheet off restores ordinary opaque styles. These properties are
available through the same C++, C and Rust stylesheet APIs on every backend;
no OS compositing/window-opacity API is involved.

C++ `StyleBox` also exposes `backgroundOpacity` and
`contentBackgroundOpacity`. `resolveBackgroundOpacity()` bakes and consumes
them once; stylesheet resolution, primitive painting and direct control-style
adapters use the same normalization. Native child renderers (e.g. embedded
Scintilla) remain responsible for their own canvas and are not made transparent
by a OneUI surface rule. The existing `opacity` property's behavior is unchanged.
Rebuild C++ consumers after upgrading because `StyleBox` has new fields; the C
ABI uses opaque stylesheet handles and has no layout change.

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
- `grid-min-column-width` for responsive `ReorderableGrid` columns
- `box-shadow`, including `inset`

Single-line controls use measured ellipsis rendering when their text exceeds
the assigned content rectangle. Products should provide semantic dimensions
through CSS and must not pre-truncate labels or replace the ellipsis contract.

### Text shaping and document tabs

Label supports `letter-spacing: normal | <px>` (including negative spacing) and
`font-variant-numeric: normal | proportional-nums | tabular-nums`. These properties
are applied by the C API style adapter to Label; they do not imply CSS inheritance
or support on every control. Measurement and drawing share the shaped layout.

Document-mode Tabs supports `tab-indicator-color`, `tab-indicator-inset`,
`tab-indicator-height`, `tab-outline-width`, and `tab-trailing-inset`.
The selected item's border color controls its outline independently of the
indicator color. The trailing inset reserves a non-clickable gutter inside each
item's allocated frame; it does not add layout spacing. All lengths are
nonnegative logical pixels. Defaults preserve the previous full-width indicator
and omit the outline. Use `content-radius` for the item's top corner radius.

This is not a complete browser CSS engine yet. The intended growth path is:

1. add design-token variables,
2. add font and layout properties,
3. add native backend painters for gradients, blur, and high-quality shadows,
4. support separate external `.oui.css` files and live reload for designers.


### Transient palette and inherited style scopes

A wallpaper-backed page can keep light foreground text while its dialogs and
menus follow the application's light theme. Use two retained `StyleSheet`s;
register the transient variant with `setTransientStyleSheet`, the C API
`oneui_style_sheet_set_transient_style_sheet`, or Rust
`set_transient_style_sheet(Some(&variant))`. The registration is weak: keep the
variant alive in the application. `replace_css` preserves the registration.

The C API applies the variant at Dialog, Popup and Menu boundaries. Retained
children, dynamically inserted controls and nested containers inherit that
sheet. Popup anchors remain in the page palette. Select uses the variant only
for its popup/options; the closed trigger retains the page palette. Refresh the
owning window after changing tokens on either sheet; mounted widget bindings
continue to refer to the declared window stylesheet for refresh ownership.

Native C++ containers expose `Widget::setStyleScope` for subtree context. A local
scope wins over an inherited one; clearing it restores the inherited scope.
View, OverlayHost and Popup propagate the context through their owned content.
Custom containers must propagate it too. Native C++ clients can use
`setOnStyleScopeChanged` to reapply their style adapter; C API clients receive
that binding automatically. This is presentation context, not CSS ancestor
selector support. Detach/reparent clears inherited scope, while explicit scope
survives. Owner callbacks are disconnected before restyling detached content.

Regression coverage: `oneui_style_scope_tests` exercises nested scopes, dynamic
insertion, reparenting, scope removal, owner destruction, anchor isolation and
Select popup state palettes. Product render fixtures should additionally test
actual photo backgrounds and the application's transient CSS.
