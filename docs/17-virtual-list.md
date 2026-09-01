# VirtualList

`VirtualList` is the standard OneUI control for a large, fixed-height list of
simple `ListItem` records or operational `VirtualListItem` records. It owns structured data and renders only the rows
that intersect its viewport. It is intentionally not a container for arbitrary
child widgets.

## Use it when

- the list has a stable row height;
- each row is a title with an optional detail line, or a rich operational row
  with badge, trailing status, and an optional colored indicator;
- data volume can grow beyond the small, always-visible lists used in settings;
- selection, keyboard navigation, and wheel scrolling are required.

Use `List` for short static choices. Use a dedicated virtualized presenter when
each row needs controls, menus, editable fields, or a variable height.

## Contracts

- The control retains data, not a widget per row.
- Paint work is bounded by visible rows plus one prefetch row above and below.
- `setSelectedIndex` keeps the selected row visible.
- `Up`, `Down`, `Home`, and `End` provide keyboard navigation.
- Mouse wheel input is consumed only while the pointer is within a scrollable
  list.
- Wheel input uses the shared 120 ms ease-out scroll transition. Repeated wheel
  events accumulate at the transition target instead of restarting from a stale
  visual position.
- `setScrollOffset` and keyboard selection remain immediate so programmatic
  positioning, accessibility navigation, and deterministic tests do not depend
  on animation timing.
- Rows use the established `ListStyleOverride` contract, so CSS uses the same
  semantic properties and states as `List`.
- Rich metrics are clamped to non-negative geometry. Narrow rows compress or
  clip badge/trailing regions before they can overlap title/detail text.
- Batch and single-row updates preserve the current scroll offset and valid
  selection. Worker handles coalesce pending row updates on the UI thread.

`ListItem` intentionally remains the shared title/detail model used by `List`.
Use `VirtualListItem` only when the row needs the additional operational fields.

## CSS

The default type selector is `virtual-list`. Assign semantic classes in the
product theme instead of putting product colors or spacing in page code.

```css
virtual-list.host-inventory {
    background: var(--surface);
    border-color: var(--border);
    border-width: 1px;
    border-radius: 8px;
    color: var(--text);
    content-background: transparent;
}

virtual-list.host-inventory:hover {
    content-background: var(--surface-hover);
}

virtual-list.host-inventory:selected {
    content-background: var(--surface-selected);
    color: var(--accent-soft);
}
```

## Rust

```rust
use oneui::{Color, VirtualList, VirtualListItem};

let list = VirtualList::new()?;
list.set_row_height(52.0);
list.set_rich_items(&[
    VirtualListItem {
        title: "ERP management".into(),
        detail: "erp-demo.example".into(),
        badge: "HTTP".into(),
        trailing: "Running".into(),
        indicator_color: Some(Color::rgba(34, 197, 94, 255)),
        trailing_color: Some(Color::rgba(22, 163, 74, 255)),
    },
]);
list.set_selected_index(0);
parent.set_content(list.as_widget());
```

## Verification

The C++ behavior suite exercises large-list bounded painting, rich-row updates,
narrow geometry, and state preservation. C ABI and Rust tests cover rich batch,
single-row, worker-handle, and metric paths. Keep all layers synchronized when
changing scrolling, painting, or ABI ownership.
