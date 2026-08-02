# VirtualList

`VirtualList` is the standard OneUI control for a large, fixed-height list of
simple `ListItem` records. It owns structured data and renders only the rows
that intersect its viewport. It is intentionally not a container for arbitrary
child widgets.

## Use it when

- the list has a stable row height;
- each row is a title with an optional detail line;
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
use oneui::{ListItem, VirtualList};

let list = VirtualList::new()?;
list.set_row_height(52.0);
list.set_items(&items);
list.set_selected_index(0);
parent.set_content(list.as_widget());
```

## Verification

The C++ behavior suite exercises a 5,000-item list and asserts a bounded paint
set. The Rust binding suite mounts the same scale of structured data through
the UTF-8 ABI. Keep both checks when changing scrolling, painting, or ABI
ownership.
