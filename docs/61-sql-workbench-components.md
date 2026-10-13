# SQL workbench components

Components added for data-heavy workbenches (the iShell Pro SQL module is the
first consumer). They are generic: nothing here knows about SQL.

## DataGrid

`DataGrid` is a virtualized, editable cell grid for result sets of any size.
The view model owns the data; the grid only holds the rows it is currently
showing (the *window*) and asks for more through the `viewport` event.

### Data model

- `row-count` is the logical number of rows (up to tens of millions; row
  positions are kept in double precision so scrolling stays pixel exact).
- `grid-rows` is the materialized window, and `window-start` is the logical
  index of its first row. Rows outside the window paint as placeholders.
- When the visible range leaves the window the grid fires
  `viewport(first, count)`; the request covers about three screens so ordinary
  scrolling does not round-trip on every frame. The VM answers by replacing
  `grid-rows` and `window-start`.
- Painting touches only visible rows and columns. A 1,000,000 × 30 grid paints
  a frame in about 0.025 ms on the recording canvas
  (`tests/data_grid_behavior_tests.cpp`).

### Declarative contract

```xml
<DataGrid :grid-columns="columns" :grid-rows="rows" :row-count="total"
          :window-start="start" :frozen-columns="frozen" :editable="editable"
          row-height="24" header-height="28" row-numbers="true"
          @viewport="viewport" @sort="sort" @resize="resize" @selection="selection"
          @edit="edit" @context="context" @copy="copy" @activate="activate"/>
```

| Property | Type | Notes |
| --- | --- | --- |
| `grid-columns` | list of `@grid.column` | `id`, `title`, `detail` (type line under the title), `width` (px, 0 = default), `align` (0 left, 1 center, 2 right), `sort` (0 none, 1 ascending, 2 descending), `key` (primary-key marker) |
| `grid-rows` | list of `@grid.row` | `id`, `number` (gutter text), `classes`, `cells` (list of records with `text` and `classes`) |
| `row-count`, `window-start`, `frozen-columns` | i64 | |
| `row-height`, `header-height` | f64 | clamped to 16–96 px |
| `row-numbers`, `editable` | bool | |
| `anchor-row`, `anchor-column`, `focus-row`, `focus-column`, `select-kind` | i64 | programmatic selection (focus -1 = none); `select-kind` 0 cells, 1 rows, 2 columns, 3 all |

Record fields are checked by `oneui-viewc` and again when the image loads;
a missing or mistyped field (including the nested cell record) is a
`BIND_RECORD` diagnostic, not a runtime surprise.

Style classes are space-separated tokens painted with theme colors:
cells `null`, `number`, `modified`, `error`, `truncated`, `muted`; rows
`inserted`, `deleted`, `modified`. A `null` cell shows its text as a muted
marker and opens the editor empty.

| Event | Arguments |
| --- | --- |
| `viewport` | `first: i64, count: i64` |
| `sort` | `column: i64` (header sort button) |
| `resize` | `column: i64, width: f64` (drag or double-click autofit) |
| `selection` | `anchorRow, anchorColumn, focusRow, focusColumn, kind: i64` |
| `edit` | `row: i64, column: i64, text: utf8` |
| `context` | `row, column: i64, x, y: f64` (window coordinates) |
| `copy` | none — the VM decides the clipboard format |
| `activate` | `row, column: i64` (read-only grids only) |

The grid never writes the clipboard and never changes its own data: sorting,
resizing, edits and copies are requests the VM answers by updating bindings.

### Interaction

- Mouse: click selects a cell, Shift+click extends, drag selects a rectangle;
  the gutter selects whole rows, the header whole columns; the header's sort
  button raises `sort`; dragging a header edge resizes, double-clicking it
  autofits to the header and visible rows; right click selects (unless inside
  the selection) and raises `context`.
- Keyboard: arrows, Home/End, Ctrl+Home/End, PageUp/PageDown, Tab/Shift+Tab,
  Shift extends; Ctrl+A selects all; Ctrl+C and Ctrl+Insert raise `copy`;
  the Menu key and Shift+F10 raise `context` under the focused cell.
- Editing (when `editable`): F2, double-click, or typing any character opens
  a text editor over the cell (typed text replaces the value). IME composition
  also opens the editor, so Chinese input works without pressing F2 first.
  Enter commits and moves down, Shift+Enter commits and moves up, Tab and
  Shift+Tab commit and move sideways, Esc cancels, Alt+Enter inserts a line
  break, and clicking elsewhere or losing focus commits.
- Read-only: double-click and Enter raise `activate`.

### C++ API

`include/oneui/controls/data_grid.h` exposes the same model directly:
`setColumns`, `setRowCount`, `setWindow(start, rows)`, incremental
`insertWindowRows` / `eraseWindowRows` / `updateWindowRow`, selection and
scrolling (`setSelection`, `scrollToCell`), editing (`beginEdit`,
`cancelEdit`) and the callbacks listed above.

## TreeView

The object navigator uses the existing `TreeView` with its large-tree
additions: internal scrolling, lazy branches with a loading placeholder,
retained expansion across incremental inserts, icons, badges, style classes,
filter highlight ranges and `expand` / `activate` / `context` events. See
[TreeView](34-tree-view.md#large-trees-lazy-branches-and-decorations).
