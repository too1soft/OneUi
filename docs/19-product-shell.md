# ProductShell

`ProductShell` is a layout-only helper for dense desktop tools: remote clients,
cloud consoles, device managers, and admin utilities. It is intentionally
product-neutral. Products supply copy, commands, icons, and native/control
implementations.

## Purpose

- Provide consistent sidebar/header/content/footer geometry.
- Provide card and form-row geometry for native shells that cannot yet render
  every control through OneUI.
- Keep early Win32-compatible products from hand-rolling one-off layout math.

## API

- `ProductShellMetrics`: reusable spacing, card, form, and breakpoint values.
- `computeProductShellLayout(...)`: computes sidebar, header, content, footer.
- `computeProductSidebarLayout(...)`: computes avatar/account, navigation,
  separator, and bottom settings geometry for product shells.
- `computeProductTopBarLayout(...)`: computes search, promotion, version
  switch, and notification geometry for product shells.
- `computeProductDashboardLayout(...)`: computes service, primary, secondary,
  and log cards.
- `computeProductFormRowLayout(...)`: computes label/control/trailing regions
  inside a card.
- `computeProductAssistHomeLayout(...)`: computes a remote-assistance landing
  surface with large local identity, temporary code action, remote target
  inputs, local device/status pill geometry, long-term password geometry, mode
  switches, and recent-device card slots.
- `computeProductStatusStripLayout(...)`: computes a compact product status
  strip with icon, summary message, copy action, disclosure action, and optional
  expanded details region.
- `computeProductWindowChromeLayout(...)`: computes client-side desktop
  chrome for native shells: title bar, caption region, min/max/close buttons,
  and content bounds.

## Remote Client Usage

The remote native shell uses `ProductShell` for its main window layout while it
continues to host Win32 controls for Win7 compatibility. This lets the product
move toward OneUI without blocking on every custom control being finished.

New remote-client UI needs should continue to follow the OneUI-first rule:
build reusable layout/control capability in OneUI first, then consume it from
the remote client.

The native remote client now uses `ProductAssistHomeLayout` for its Sunlogin-like
first screen: persistent navigation on the left, a compact top service bar, a
large local device code, a single dominant connect action, and reusable recent
device card slots. The layout is still product-neutral; products provide the
visual treatment and text.

The sidebar and top bar are also modeled in OneUI now. Remote should use those
rectangles for Sunlogin-like desktop chrome instead of baking pixel positions
into its Win32 shell.

It also uses `ProductWindowChromeLayout` to keep custom title bars reusable
across Win7-compatible tools without tying OneUI to a specific product name or
window manager implementation.
