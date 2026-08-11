# Reorder Contract

OneUI owns input interpretation and insertion geometry. Products own domain validation, persistence, and the accepted order. This separation is shared by `VirtualList`, `TreeView`, and `ReorderableGrid`.

## Semantics

- A pointer move shorter than 4 logical pixels remains a normal click.
- A drag suppresses the child activation that would otherwise follow mouse-up.
- The callback fires once on release and reports the final position after removal of the source item.
- `VirtualList` reports source and final target indices.
- `TreeView` reports stable source and target IDs; the product decides whether parent and sibling rules allow the move.
- `ReorderableGrid` reports a stable source ID and final target index.
- Cross-control item drag is opt-in and independent from internal reorder. `VirtualList` and `ReorderableGrid` report `Started`, `Updated`, `Dropped`, and `Cancelled` phases in client-space coordinates while preserving the stable source ID.
- `VirtualList` accepts a parallel set of non-empty, unique domain IDs. The ID count must equal the current row count, and replacing the full row model clears the IDs so stale identities cannot escape virtualization.
- `TreeView` owns only the transient external drop-target presentation. The product validates the returned stable target ID and applies the domain move once on drop.
- Selection is not silently changed by a reorder request.
- `Alt+Up` and `Alt+Down` provide the keyboard reorder path for list and tree controls.

## Product Integration

1. Keep stable, non-empty, unique IDs separate from rendered labels and current indices. `VirtualList` and `ReorderableGrid` reject ambiguous IDs at their boundaries.
2. Validate the request against product rules.
3. Persist the accepted domain order.
4. Apply the accepted order in place (`ReorderableGrid::moveItem`) or replace the data model once.
5. Do not destroy the callback-owning control from inside its own native callback.

For cross-control drag, call `TreeView::updateExternalDropTarget` during the start/update phases, read `externalDropTargetId()` on drop, and always call `clearExternalDropTarget()` on drop or cancellation. The safe Rust equivalents use snake-case names. An empty target ID means the pointer is outside a visible tree row.

The C ABI copies callback arguments for the duration of the call. Safe Rust wrappers own callback storage, catch panics at the ABI boundary, unregister native callbacks before dropping storage, and expose UTF-8 values as owned Rust strings.

## Styling And DPI

Reorder indicators use `outline-color`, `outline-width`, and `text-inset` where applicable. `ReorderableGrid` also consumes `gap`, `height`, `padding`, and the optional `grid-min-column-width`. Its API column count is the maximum; layout, content height, hit testing, and insertion geometry all use the same width-derived effective column count. These values are logical device-independent units and are converted to physical pixels by the per-monitor DPI-aware Win32 backend.

## Performance

- Pointer movement updates only transient reorder state and invalidates the affected control.
- Cross-control drag never scans or mutates product data on pointer movement; target hit testing is limited to the receiving control's visible rows.
- Accepted grid moves reorder existing child handles in place.
- `VirtualList` continues to paint and hit-test only visible rows, does not allocate one widget per item, and validates stable drag IDs only when its model changes rather than during scrolling or pointer updates.
- Products should persist once on release, not on every pointer move.

## Required Tests

Changes to reorder behavior must cover below-threshold clicks, drag suppression, stable IDs, final-index semantics, keyboard behavior, CSS indicator geometry, callback cleanup, external drop-target cleanup, and at least one non-symmetric padding case.
