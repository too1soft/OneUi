# Remote OneUI CSS ABI Implementation

> 本文是已落地 C ABI 的实现清单（快照，随代码增长可能滞后，实际以 `include/oneui/oneui_c_api.h` 头文件为准）。ABI 的设计总纲与红线见 `c-abi-integration.md`。

Remote must treat OneUI as the owner of UI rendering, component state, and CSS-like styling.
The product layer may provide text, data, event callbacks, and layout composition, but it must
not implement reusable visual behavior such as button colors, input focus rings, card shadows,
or disabled/read-only state styling.

## Implemented ABI

The first stable C ABI slice for downstream products is:

- `oneui_style_sheet_create`
- `oneui_style_sheet_destroy`
- `oneui_style_sheet_set_custom_property`
- `oneui_style_sheet_add_css`
- `oneui_style_sheet_load_file`
- `oneui_window_set_style_sheet`
- `oneui_widget_set_classes`
- `oneui_widget_set_style_node`
- `oneui_widget_apply_style_sheet`
- `oneui_app_shell_create`
- `oneui_app_shell_set_sidebar`
- `oneui_app_shell_set_header`
- `oneui_app_shell_set_content`
- `oneui_app_shell_set_footer`
- `oneui_app_shell_set_sidebar_width`
- `oneui_app_shell_set_header_height`
- `oneui_app_shell_set_footer_height`
- `oneui_app_shell_set_gap`
- `oneui_app_shell_set_padding`
- `oneui_app_shell_set_sidebar_visible`
- `oneui_top_bar_create`
- `oneui_top_bar_set_leading`
- `oneui_top_bar_add_action`
- `oneui_top_bar_clear_actions`
- `oneui_top_bar_set_gap`
- `oneui_top_bar_set_padding`
- `oneui_top_bar_set_leading_width`
- `oneui_product_shell_create`
- `oneui_product_shell_set_sidebar`
- `oneui_product_shell_set_topbar`
- `oneui_product_shell_set_content`
- `oneui_product_shell_set_status`
- `oneui_product_shell_set_sidebar_width`
- `oneui_product_shell_set_topbar_height`
- `oneui_product_shell_set_status_height`
- `oneui_product_shell_set_gap`
- `oneui_product_shell_set_padding`
- `oneui_product_shell_set_sidebar_visible`
- `oneui_window_minimize`
- `oneui_window_toggle_maximize`
- `oneui_window_set_borderless`
- `oneui_window_request_animation_frame`
- `oneui_icon_create`
- `oneui_icon_set_symbol`
- `oneui_icon_set_color`
- `oneui_icon_set_accent`
- `oneui_icon_set_stroke_width`
- `oneui_icon_button_create`
- `oneui_icon_button_set_symbol`
- `oneui_icon_button_set_on_click`
- `oneui_switch_create`
- `oneui_switch_set_text`
- `oneui_switch_set_checked`
- `oneui_switch_checked`
- `oneui_switch_set_on_changed`
- `oneui_text_field_set_prefix_icon`
- `oneui_text_field_clear_prefix_icon`
- `oneui_text_field_set_suffix_icon`
- `oneui_text_field_clear_suffix_icon`
- `oneui_search_box_create`
- `oneui_title_bar_create`
- `oneui_title_bar_set_title`
- `oneui_title_bar_set_icon_symbol`
- `oneui_title_bar_set_maximized`
- `oneui_title_bar_set_on_minimize`
- `oneui_title_bar_set_on_maximize`
- `oneui_title_bar_set_on_close`
- `oneui_nav_item_create`
- `oneui_nav_item_set_text`
- `oneui_nav_item_set_symbol`
- `oneui_nav_item_set_selected`
- `oneui_nav_item_set_on_click`
- `oneui_badge_create`
- `oneui_badge_set_text`
- `oneui_badge_set_variant`
- `oneui_card_create`
- `oneui_card_set_content`
- `oneui_tile_create`
- `oneui_tile_set_title`
- `oneui_tile_set_subtitle`
- `oneui_tile_set_leading_symbol`
- `oneui_tile_clear_leading_symbol`
- `oneui_tile_set_trailing_symbol`
- `oneui_tile_clear_trailing_symbol`
- `oneui_tile_set_on_click`
- `oneui_status_strip_create`
- `oneui_status_strip_set_title`
- `oneui_status_strip_set_message`
- `oneui_status_strip_set_primary_action`
- `oneui_status_strip_set_secondary_action`
- `oneui_status_strip_set_on_primary_action`
- `oneui_status_strip_set_on_secondary_action`
- `oneui_toast_create`
- `oneui_toast_set_title`
- `oneui_toast_set_message`
- `oneui_toast_set_primary_action`
- `oneui_toast_set_secondary_action`
- `oneui_toast_set_icon_symbol`
- `oneui_toast_clear_icon_symbol`
- `oneui_toast_set_close_visible`
- `oneui_toast_set_on_primary_action`
- `oneui_toast_set_on_secondary_action`
- `oneui_toast_set_on_close`
- `oneui_overlay_host_create`
- `oneui_overlay_host_set_content`
- `oneui_overlay_host_add_overlay`
- `oneui_overlay_host_add_anchored_overlay`
- `oneui_radio_group_create`
- `oneui_radio_group_set_items`
- `oneui_radio_group_set_selected_index`
- `oneui_radio_group_selected_index`
- `oneui_radio_group_set_orientation`
- `oneui_radio_group_set_on_changed`

These APIs allow Rust, Go, C, and other hosts to load a OneUI DLL, create a stylesheet,
attach it to a window, and assign tag/class names to generic OneUI widgets.

## Current CSS Subset

The supported subset is intentionally small and product-oriented:

- `:root` custom properties such as `--color-surface: #111114`
- `var(--name)` and `var(--name, fallback)` value substitution
- Unknown `var(--name)` references without a fallback fail parsing with a diagnostic
- `background` and `background-color`
- `linear-gradient(...)` parsing for gradient tokens
- `rgb(...)` and `rgba(...)` colors for product-grade shadows, outlines, and state layers
- `color`
- `placeholder-color`
- `caret-color`
- `selection-color`, `selection-background`, and `selection-background-color`
- `border` shorthand
- `border-color`
- `border-width`
- `border-radius`
- `outline` shorthand
- `outline-color`
- `outline-width`
- `outline-offset`
- `opacity`
- `padding`
- `gap`
- `width`
- `height`
- `font-size`
- `font-weight`
- `transition-duration`
- `transition-timing-function` (`linear`, `ease-out`, `ease-in-out`)
- `box-shadow`
- `content-background`
- `content-radius`
- `content-inset`

Selectors support tags, classes, multiple classes, comma groups, and pseudo states:

- `:hover`
- `:active`
- `:focus`
- `:disabled`
- `:selected`
- `:read-only`

## Component Mapping

The C ABI maps style nodes to reusable components as follows:

- `Button` uses `buttonStyleOverrideFromStyleSheet` and consumes CSS shadows for raised/pressed states.
- `TextField` uses `textFieldStyleOverrideFromStyleSheet`, consumes CSS shadows for field depth/inset polish, and can render prefix/suffix icons from the OneUI icon registry.
- `SearchBox` is a reusable TextField-derived component with standard search prefix and dropdown/clear affordance slots.
- `Panel` uses a generic `StyleBox` plus padding for child layout.
- `Label` consumes foreground color and font size.
- `Stack` consumes a generic `StyleBox`, including background, border, radius, padding,
  gap, and shadows. Layout containers must be able to paint their own surfaces so hosts
  do not add fake panels merely to avoid white fallback backgrounds.
- `TopBar` owns a generic leading/action strip layout for product headers. It does not know
  about search, upgrade prompts, mode segments, or notifications; those are just child widgets.
  It also consumes a generic `StyleBox` for header surfaces.
- `AppShell` owns reusable product shell slot layout: sidebar, header, content, footer,
  shell padding, gap, sidebar visibility, and generic `StyleBox` surface painting.
- `ProductShell` is the public product-frame ABI over the same reusable slot layout:
  sidebar, topbar, content, and status. Product hosts should prefer this semantic API over
  raw `AppShell` when building full desktop app shells.
- `IconView` consumes foreground color, accent color, and stroke width.
- `IconButton` owns icon-only command painting, icon registry usage, hover/pressed/disabled
  state handling, focusability, and click dispatch.
- `Switch` is exposed through C ABI and consumes CSS/token state styles.
- `WindowTitleBar` owns product-window chrome painting, title text, brand icon, minimize/maximize/close buttons, and button hover/active states.
- `NavItem` owns sidebar navigation item painting, icon/text layout, selected/hover/pressed/disabled states, and click dispatch.
- `Badge` owns compact pill/status painting, border/radius/padding/font token consumption, and variant fallback styling.
- `Card` owns card shell painting, shadow/fill/border/radius/padding style consumption, and child content layout.
- `Tile` owns reusable clickable tile painting, title/subtitle typography, leading/trailing
  icon registry usage, hover/pressed/disabled/focus states, and click dispatch.
- `StatusStrip` owns reusable status surface painting, title/message typography, action button
  hit testing, hover/pressed states, action callbacks, and CSS-addressable action subparts
  through generic classes such as `status-strip-action`, `primary`, and `secondary`.
- `RadioGroup` owns real mode selection, keyboard/mouse selection behavior, horizontal/vertical item layout, and selected/hover/pressed state styling.
- `SegmentedControl` is exposed through the C ABI over the generic `Tabs` core. It owns compact
  two-or-more option selection, selected/hover/pressed/focus/disabled state styling, and callback dispatch.
- `Toast` owns reusable floating notice painting: title/message text, optional icon, primary/secondary
  actions, close affordance, hit testing, shadow/fill/border/radius/padding style consumption,
  callbacks, and CSS-addressable action subparts through generic classes such as
  `toast-action`, `primary`, `secondary`, and `close`.
- `OverlayHost` owns generic layering: base content, modeless overlays, anchored overlay placement,
  layer ordering, hit testing, and focus handoff. It is the required path for floating notices instead
  of faking vertical layout rows in product code.
- `Canvas::fillLinearGradient` and `paintStyleBox` now render CSS-like `linear-gradient(...)`.
- Win32 `Canvas::drawBoxShadow` uses a blurred Skia image filter instead of hard rectangle shadows.
- Win32 `Window` exposes animation frame scheduling through a Win7-compatible timer-backed
  `requestAnimationFrame` path. This is the foundation for OneUI-owned hover/focus/pressed
  transitions; Remote must not implement its own visual timers.
- Win32 `Window` owns OS cursor dispatch via widget `CursorKind`; app shells must not set
  platform cursors themselves. Text fields, buttons, nav items, and other interactive roles
  expose cursor semantics through reusable OneUI widgets.
- Win32 `Window` caches its Skia raster surface per client size. This avoids allocating a full
  backing surface on every hover repaint and keeps resize/fullscreen repaint behavior bounded.
- `Widget` and `View` propagate an animation scheduler through the component tree, so reusable
  controls can request frame callbacks without depending on a concrete platform window.
- `Button`, `TextField`, `IconButton`, and `NavItem` now own their hover/pressed/focus/disabled
  color transitions inside OneUI. Hosts without an animation scheduler still receive immediate
  state changes, keeping unit tests and non-window embedding deterministic.
- Transitions prime the first visual frame, so hover/pressed/focus changes do not paint one stale
  frame before the next timer tick.

## Animation Foundation

OneUI now has a first reusable animation primitive layer:

- `TransitionSpec` for duration/easing tokens.
- `EasingCurve` with `Linear`, `EaseOutCubic`, and `EaseInOutCubic`.
- `FloatTransition` and `ColorTransition` for component state interpolation.
- `oneui_window_request_animation_frame` for host-language frame scheduling through the C ABI.

This is intentionally generic. It contains no Remote business state and does not assume a
specific component. The next step is wiring component state styles, such as Button/TextField
hover and focus, to these transitions inside OneUI.

Remote-specific names are forbidden in OneUI. A downstream app may use classes such as
`primary`, `sidebar`, or `status-card`, but OneUI must treat them as opaque selectors.

## Remote Integration Rule

The Remote Rust shell now loads `assets/remote-home.oui.css` and assigns class names through
`oneui_widget_set_style_node`. Remote no longer owns per-control button or text-field style
structs. Any further visual capability needed by Remote should be added to OneUI first, then
used from Remote through the C ABI.

## Interaction Performance Red Lines

Remote-class desktop shells are hover-heavy. The UI must feel attached to the pointer, so these
rules are mandatory for OneUI controls and platform backends:

- Mouse move dispatch must hit-test the current topmost child, not broadcast to every sibling.
  A fast sweep over adjacent controls may invalidate only the previous hovered control and the
  newly hovered control.
- Leaving a hover target must collapse the old hover state immediately. Hover exit animations are
  forbidden for navigation and other dense controls because they leave stale highlighted rows behind
  the pointer.
- `clearInteractionState()` must be a no-op when a control has no hover/pressed/open interaction
  state. Idle controls must not repaint because a sibling changed hover state.
- Visual transitions may schedule animation frames only when an actual transition is running.
  Zero-duration or same-target transitions must not post frame callbacks.
- Animation frame dispatch must not turn every running transition into a full-window repaint.
  Controls are responsible for invalidating their own dirty rects during `tickAnimations()`;
  the platform scheduler only posts the next frame.
- Win32 painting must avoid unnecessary background erase, must reuse the raster surface while the
  client size is stable, and must blit only the invalidated paint rectangle when possible.
- Interactive state changes must flush the pending paint promptly. Pointer feedback must not wait
  behind a long queue of mouse-move messages.
- Hover exit is an interactive state change. When a pointer leaves a control or moves into blank
  space, the old highlight must be reported as handled so the platform can flush the repaint
  immediately.
- Widget invalidation should carry a dirty rectangle whenever the changed visual area is known.
  View containers must propagate that rectangle upward instead of converting every child hover into
  a full-window repaint.
- Partial paints must expose clip bounds to the widget tree so `View` can skip children outside the
  dirty region. Clipping only at the platform canvas is not enough because offscreen controls would
  still run text measurement and shadow setup work.
- Style resolution must cache resolved `tag + classes + pseudo-state` boxes. Components may prewarm
  hot hover/pressed/focus states when a stylesheet is attached, especially for sidebar nav rows,
  icon buttons, and tiles.
- Hot controls may keep component-local state style caches, but those caches must be invalidated
  by the stylesheet version so dynamic CSS updates cannot leave stale visuals.
- Platform text rendering must cache resolved typefaces by weight. Looking up system fonts during
  hover paints is forbidden.
- Platform text rendering must avoid measurement work when it is not required. Left-aligned text
  should not call expensive text-width measurement merely to compute an x position.
- Platform cursor handling must cache system cursor handles and reuse the current widget cursor
  result for duplicate `WM_SETCURSOR` messages at the same client coordinate.
- Remote page CSS should avoid heavy blur shadows on elements that repaint during hover. Dense
  hover affordances such as sidebar nav items should use immediate state changes.

Remote also uses OneUI `IconView` through `oneui_icon_create` for the product mark,
sidebar navigation, and recent-connection cards. Remote may choose an icon symbol and pass
business text, but it must not draw icon primitives itself.

Remote uses OneUI `IconButton` through `oneui_icon_button_create` for toolbar-style icon
commands such as notifications. Remote may choose the symbol and callback; hit testing,
hover/pressed visuals, focusability, and icon painting belong to OneUI.

Remote uses OneUI `SearchBox` through `oneui_search_box_create` for the top search/service
entry. Remote may provide the current text, placeholder, and classes; search icon placement,
suffix affordance, text layout, caret offset, selection region, and icon painting stay inside
OneUI.

Remote uses OneUI `TopBar` through `oneui_top_bar_create` for the header action strip. Remote
may set the leading widget and append action widgets; horizontal measurement, right-aligned
actions, spacing, and future responsive collapse belong to OneUI.

Remote uses TextField affix icon ABI for the remote-device-code field and verification-code
field. A downstream app may choose icon symbols, but the text layout, caret offset, selection
region, and icon painting stay inside OneUI.

Remote uses OneUI `WindowTitleBar` through `oneui_title_bar_create` for the custom chrome.
Remote may only provide the title string, product icon symbol, and window action callbacks.
Titlebar button sizing, hit-testing, icon drawing, hover/pressed states, and Win7-compatible
chrome fallback belong to OneUI.

Remote uses OneUI `ProductShell` through `oneui_product_shell_create` for the main product
layout. Remote may provide sidebar/topbar/content/status widgets and slot metrics; slot
measurement, sidebar/content rects, shell padding, and future responsive behavior belong to
OneUI.

Remote uses OneUI `NavItem` through `oneui_nav_item_create` for sidebar navigation rows.
Remote may only provide label text, icon symbol, selected state, and callbacks. Sidebar item
backgrounds, foreground colors, icon/text positioning, hit-testing, and hover/pressed states
belong to OneUI.

Remote uses OneUI `Badge` through `oneui_badge_create` for compact topbar pills. Remote may
provide text, variant, and classes; fill, border, radius, padding, and typography remain OneUI
style concerns.

Remote uses OneUI `Tile` through `oneui_tile_create` for recent connection tiles. Remote may
provide title, subtitle, symbols, and callbacks; tile background, border, shadow, radius,
padding, typography, icon positioning, and hover/pressed states belong to OneUI.

Remote uses OneUI `StatusStrip` through `oneui_status_strip_create` for bottom status
messaging. Remote may update title/message/action labels and callbacks; the status surface,
action hit testing, hover/pressed states, and compact layout belong to OneUI.

Remote uses OneUI `RadioGroup` through `oneui_radio_group_create` for mode selection such as
remote desktop vs remote file. Remote must not fake selectable options with labels.

Remote uses OneUI `SegmentedControl` through `oneui_segmented_control_create` for compact
mode/version toggles. Remote may provide item labels and selected index; all visual states remain
OneUI-owned.

Remote uses OneUI `Toast` through `oneui_toast_create` for floating product notices. Remote may
provide title/message/action labels and callbacks, but the notice surface, icon drawing, action
hit testing, close affordance, shadow, and state visuals belong to OneUI.

Remote uses OneUI `OverlayHost` through `oneui_overlay_host_create` and
`oneui_overlay_host_add_anchored_overlay` for floating notices. Remote must not push toast-like
surfaces into normal stacks merely to approximate overlay placement.

Remote styles must prefer `:root` tokens in `.oui.css` for repeated colors, focus rings,
surface tones, and brand/accent values. Hard-coded colors may appear only inside a concrete
token declaration or one-off illustration/asset rule. Reusable OneUI components must never
contain Remote-specific token names or business classes.

Remote screenshot smoke test is kept at
`E:\project\byname\remote\scripts\test-oneui-shell-screenshot-smoke.ps1`. Any Remote UI slice
must rebuild OneUI, rebuild the Rust shell, and capture a smoke image before being considered done.

The OneUI-owned Remote component gallery is kept at
`E:\project\byname\oneui\examples\remote_component_gallery`. It is a C ABI gallery for the
component set required by Remote: borderless title bar, sidebar nav, topbar/search/actions,
buttons, TextField, Switch, RadioGroup, Tile, and StatusStrip. Its smoke test is:

```powershell
powershell -ExecutionPolicy Bypass -File E:\project\byname\oneui\scripts\test-remote-component-gallery-smoke.ps1 -ExerciseHover
```

The latest smoke checks for a dark product surface, nonblank rendering, and blue/purple
accent surfaces. This gallery must pass before Remote-specific visual changes are accepted.

## Next Required Slices

- Continue replacing per-control visual transition code with the reusable `StyleBoxTransition`
  primitive. Button, TextField, IconButton, NavItem, and WindowTitleBar buttons now animate
  through OneUI's frame scheduler; Tile/Toast/StatusStrip are the next composite controls.
- Extend `StyleBoxTransition` from background/foreground/border/opacity into shadow transitions
  so hover and pressed elevation can be product-grade instead of instant.
- More complete TextField IME, selection, caret, read-only, and focus behavior tests.
- Multi-window style context instead of the current single-window default stylesheet fallback.
- Broaden token usage across the Remote CSS file.
