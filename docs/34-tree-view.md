# TreeView

`TreeView` is OneUI's native hierarchical navigation component. It presents
structured groups without embedding a browser, inventing delimiter encodings,
or coupling a product's domain model to the UI library.

## Contract

Every `TreeItem` has a non-empty, unique `id`. `parentId` is optional: an
empty value makes the item a root. Items whose parent is missing, references
itself, or participates in a cycle remain visible as roots rather than making
the control invalid.

The component owns two pieces of view state:

- the selected `id`;
- expanded or collapsed state for nodes with children.

Selection is ID-based, so products can refresh data without translating a
visual row index back into a domain identifier. Expansion callbacks report the
item ID and new state after the visible rows and `contentHeight()` update, so
an enclosing `ScrollView` can synchronize its content height without reading
tree internals. Keyboard navigation follows
standard tree behavior: Up/Down move between visible rows, Right expands or
enters a branch, Left collapses or moves to the parent, and Enter/Space toggles
the selected branch. The expand marker is independently clickable.

## C++ API

```cpp
auto groups = std::make_shared<oneui::TreeView>();
groups->setItems({
    {L"ops", L"", L"Operations", L"12", true},
    {L"prod", L"ops", L"Production", L"8", true},
    {L"stage", L"ops", L"Staging", L"4", true},
});
groups->setOnSelectionChanged([](const std::wstring& groupId) {
    // Query or filter the product model by its stable group ID.
});
```

`TreeViewStyleOverride` is an alias of the existing `ListStyleOverride`, so
products can use the same normal, hover, selected, disabled, and focus styling
contract as `List` while retaining tree behavior.

## C and Rust ABI

The UTF-8 ABI uses `OneUiTreeItemUtf8` with explicit `id`, `parent_id`,
`title`, `detail`, and `expanded` fields. It copies values before returning.
`oneui_tree_view_selected_id_utf8` follows the common size-query pattern: call
it with a null buffer to get the number of bytes including the final NUL.

The safe Rust binding exposes `TreeView` and `TreeItem` with `set_items`,
`set_selected_id`, `selected_id`, and owned selection and expansion callbacks.
Callback panics never cross the C ABI, and callbacks are cleared before the
native widget is destroyed.

External drag sources may use `update_external_drop_target(x, y)`,
`external_drop_target_id()`, and `clear_external_drop_target()`. Coordinates
are client-space logical units shared by controls in the same OneUI window.
The target is transient UI state only: TreeView never moves product records,
and it reuses the configured hover style instead of introducing a separate
hard-coded drag color.

`contentHeight()` / `oneui_tree_view_content_height` / `TreeView::content_height`
report the height of the currently visible rows. When a tree is put inside a
`ScrollView`, use this value as the content height after changing its items or
expansion state.

## Large trees, lazy branches and decorations

These C++ additions are opt-in; trees that do not use them behave as before.

- `setInternalScrolling(true)` keeps the preferred height fixed and scrolls
  inside the frame (wheel, keyboard, `setScrollOffset`, `scrollToId`). Visible
  rows are cached and only rows inside the frame are painted, so 50,000
  visible nodes cost the same per frame as fifty. Home/End and PageUp/PageDown
  are available in every mode.
- `TreeItem::hasChildren` shows an expand marker before children exist;
  expanding reports `onExpansionChanged` so the product can load them. While
  `loading` is set on an expanded branch without children, a placeholder row
  shows `setLoadingText` (default `Loading...`); keyboard navigation skips it.
- `setRetainExpansion(true)` keeps the user's expand/collapse state across
  `setItems` for items whose supplied `expanded` value did not change. Products
  can therefore insert children without echoing expansion back, while a
  changed `expanded` value (for example "collapse all") still wins.
- `icon`, `badge`, `classes` (`muted`, `error`, `warning`, `accent`, `strong`)
  and a highlighted title range (`highlightStart` / `highlightLength`, UTF-16)
  decorate rows; `setRowHeight` sets the row height (default 32).
- `setOnActivated` makes Enter and double-click activate a node (the marker,
  Space and Left/Right still toggle). `setOnContextMenu` reports right clicks
  (pointer position), the Menu key and Shift+F10 (below the row), in window
  coordinates.

## Declarative contract

```xml
<TreeView :tree-items="nodes" v-model="selected" row-height="26"
          loading-text="Loading..."
          @expand="expand" @activate="activate" @context="context"/>
```

`tree-items` is a list of `@tree.node` records with `id`, `parent`, `text`,
`detail`, `icon` (an authoring icon name), `badge`, `classes`, `has_children`,
`expanded`, `loading`, `highlight_start` and `highlight_len` (the range is in
Unicode scalar values). `v-model` binds the selected id (`selectedKey`;
`selected-id` sets the same selection one way). Events:
`expand(id: utf8, expanded: bool)`, `activate(id: utf8)` and
`context(id: utf8, x: f64, y: f64)`. A tree bound through `tree-items` uses
internal scrolling and retained expansion, so a list INSERT of children keeps
the selection, the scroll position and the expanded branch. It sizes like any
leaf: give it `grow="1" basis="0"` to fill its column.

The older `items` property (`@tree.item`: `id parent title detail expanded`,
with a writable `expanded`) remains for small trees that grow to their content
height inside a `Scroll`.

## Scope

`TreeView` is intended for navigation and grouped data, such as host groups,
folders, policies and database object navigators. It intentionally does not
implement inline editing or checkboxes. It supports internal
reorder requests and external drop-target presentation, but domain validation
and persistence remain product responsibilities and must not be smuggled into
the navigation primitive.
