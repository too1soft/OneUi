# Native workspace composition

`oneui::layout::workspace` supplies a protocol-independent layout model keyed by
application IDs. `workspace_surface` mounts that model using native `SplitView`,
`Panel`, and `OverlayHost` widgets. It does not recreate application content.

## Model and view

- `DockWorkspace` owns a recursive dock tree, floating bounds/order, hidden-pane
  restore positions, and maximized ID. `snapshot`/`restore` provide checkpoints.
- `dock(id, target, edge)` splits only the target leaf; neighboring branches and
  unrelated floating windows are preserved. `hide`/`show` restore the previous
  location where possible and fall back to a surviving target when necessary.
- `resize_floating` supports eight edges/corners, fixed opposite anchors and
  application-defined minima. `clamp_floating` is a display projection: it does
  not overwrite stored desktop geometry when a window becomes smaller.
- Register content once in `DockSurface`. Call `mount` on topology/visibility
  changes and `update_floating` for movement/resize frames. The latter updates
  anchored overlay geometry without removing focused content.
- `DockPaneMinimum` distinguishes docked from floating minima. Compact mode is
  a single-pane view of the same tree, not a destructive layout conversion.
- All validation and native split allocation occur before replacing the old
  scene. Content wrappers keep their identity across tree changes. A weak focus
  bookmark restores the same visible, mounted input leaf after reparenting.

Split callbacks carry a tree path, ratio and committed flag. Update the model on
previews; persist only commits. Do not remount the tree in a live ratio callback.
The native divider supports pointer dragging, keyboard arrows, Home/End, double
click to balance, and Escape to roll a drag back without a commit. Pointer
capture loss retains the established SplitView commit-current-ratio behavior.

## Lifetime and responsibility

Keep the content's Rust controls/controllers alive normally so their callbacks
remain valid. `FocusBookmark` is weak and UI-thread-only; restoring never raises
an OS window and fails for destroyed, unmounted, hidden or disabled targets.
DockSurface likewise belongs to the UI thread.

General-purpose dragging, docking and floating-window interaction belong to
OneUI as an independent UI foundation. Applications provide content, visual
styles, business policy and persistence storage integration. The model, native
surface, gesture helpers and resize frame exist today; integrating them into a
complete reusable controller remains OneUI work. Until that integration is
available, callers explicitly wire coordinates, preview rendering and commit
callbacks. This is a current API limitation, not a permanent application-layer
responsibility. Pinned floats are layered within the workspace; cross-process
always-on-top windows are not supplied by this module. Content components own
their scrolling and narrow-width adaptation.

## Verification

- `workspace_layout` Rust tests cover relative placement, hide/restore,
  invalid edit atomicity, minima, all resize directions, pin ordering and
  externally restored floating panes.
- `oneui-main-thread-tests` filters `dock_surface` and `focus_bookmarks` create
  actual native windows and verify content identity, input retention, focus
  through docking/floating/resize, and rejection of invalid mounts.
- `oneui_control_behavior_tests` covers divider focus routing, keyboard
  adjustment, double-click balancing, Escape cancellation and owner disposal.

These tests establish the toolkit slice, not any consuming product's visual
fidelity or end-to-end protocol behavior.


## Pointer gestures and floating resize

`InteractiveSurface::set_on_drag` emits Started/Updated/Dropped/Cancelled in
window coordinates. A subthreshold gesture is still a click; completed drags
do not click. Escape, disabling, and pointer-reset cancel the gesture. Defer
reparenting until after the input callback has returned.

`workspace_gesture` supplies pure directional hit testing and a non-destructive
`DockDrag` checkpoint. The current helper API requires callers to convert window
coordinates to workspace coordinates, render the preview, and invoke commit or
restore. Floating sources use strict edge/central-guide hit testing; docking can
be disabled for a gesture, for example while Alt is held.

`FloatingFrame` keeps the content widget and adds eight resize targets. Call
`refresh` with current native bounds and floating visibility. Only the southeast
handle participates in keyboard focus; scoped arrow commands emit a 10px resize,
or 40px with Shift. Hidden/docked commands do not execute. `set_surface_classes`
selects the backing panel's docked/floating theme classes without replacing the
content. Applications supply the theme classes. Reusable resize readouts,
platform cursor parity and end-to-end native input acceptance remain pending.

Additional verification covers gesture regions in `workspace_layout`, native
content/focus/resize command scope, and C++ drag lifecycle checks. These remain
toolkit and fixture tests, not a completed OS-input acceptance run.


`pane_drop` supports nested leaf rearrangement independently from module docking:
normalized nearest-edge hit testing and the central 44% swap area. It excludes
the source and returns a preview rectangle without changing the tree. Applications
must validate their original tree at commit and keep the terminal/session object.
`DockNode::minimum` is public for consumers that need a scrolling canvas instead
of compressing nested leaves below their content minimum. The shared model suite
includes leaf hit regions and self/outside rejection.
