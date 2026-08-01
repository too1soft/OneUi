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
visual row index back into a domain identifier. Keyboard navigation follows
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
`set_selected_id`, `selected_id`, and an owned selection callback. Callback
panics never cross the C ABI, and the callback is cleared before the native
widget is destroyed.

`contentHeight()` / `oneui_tree_view_content_height` / `TreeView::content_height`
report the height of the currently visible rows. When a tree is put inside a
`ScrollView`, use this value as the content height after changing its items or
expansion state.

## Scope

`TreeView` is intended for navigation and moderately sized grouped data, such
as host groups, folders, and policies. It intentionally does not implement
editing, drag-and-drop, checkboxes, or large-data virtualization yet; those are
separate components and should not be smuggled into a navigation primitive.
