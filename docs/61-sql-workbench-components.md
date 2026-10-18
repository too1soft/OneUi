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

## CodeEditor

`CodeEditor` is a native multi-line code editor drawn by OneUI (no embedded
browser or child HWND, so popups and dialogs layer over it normally).

### Model

- The editor owns the text. Storage is one buffer per line with cached UTF-8
  line lengths and lazily rebuilt prefix sums, so edits cost the touched lines
  and 100,000-line scripts stay interactive (measured with the recording
  canvas: open including lexing and first paint about 70 ms, a keystroke at the
  end including paint about 0.3 ms, a wrapped keystroke about 0.5 ms;
  `tests/code_editor_behavior_tests.cpp`).
- Offsets at the product boundary are UTF-8 byte offsets; line and column are
  1-based, the column in Unicode scalar values.
- Undo groups consecutive typing (and consecutive deletes) of one cursor within
  a second; a newline, a command or a pause starts a new step. Multi-cursor
  edits, Replace All and line commands are single steps.

### Language description

Syntax colors are computed in C++ from a JSON description; colors derive from
theme tokens, so every theme gets matching colors without extra tables.

```json
{
  "case_sensitive": false,
  "keywords": ["SELECT", "FROM"], "types": ["INT"], "functions": ["COUNT"],
  "constants": ["NULL", "TRUE"],
  "line_comments": ["--", "#"], "block_comments": [["/*", "*/"]],
  "nested_block_comments": false,
  "string_quotes": "'\"", "backslash_escapes": true,
  "identifier_quotes": ["``", "\"\"", "[]"],
  "parameter_prefixes": ":@?", "dollar_quotes": true
}
```

Unknown keys are ignored; malformed descriptions are rejected. Lexing is
incremental: only line end states are kept for the whole document, tokens are
produced for painted lines, and after an edit lexing stops at the first line
whose end state did not change. Words followed by `(` color as functions.

### Declarative contract

```xml
<CodeEditor name="sql" :language="language" :text="script" :text-revision="scriptRevision"
            :read-only="readOnly" word-wrap="false" line-numbers="true" indent-guides="true"
            tab-size="4" insert-spaces="true" font-size="13" comment-prefix="--"
            @changed="changed" @cursor="cursor"/>
```

- `text` with `text-revision`: the VM replaces the document by publishing new
  text with a larger revision; equal or older revisions are ignored, so a late
  property refresh never reverts typing. With revision 0 the text is initial
  content and applies only until the user edits.
- `changed(revision: i64)` and `cursor(caret, anchor, line, column, cursors: i64)`
  are throttled: the first change after a quiet period is reported at once and
  further ones at most every 150 ms.
- The VM reads text on demand instead of mirroring every key:
  `oneui_view_code_editor_state`, `oneui_view_code_editor_text` (byte range,
  size query with a NULL buffer), `oneui_view_code_editor_replace` (one undoable
  edit; `expected_revision` turns a race with typing into `STALE_CHANGE`) and
  `oneui_view_code_editor_select`. Rust: `View::code_editor_state`,
  `code_editor_text`, `code_editor_replace`, `code_editor_select`. The element
  is addressed by its `name`.

### Decorations

```xml
<CodeEditor :decorations="marks" :decorations-revision="marksRevision" .../>
```

`@editor.decoration` records carry `id`, `start` and `end` (UTF-8 bytes),
`kind` (`statement` background, `error` / `warning` squiggles, `running`
gutter bar and tint, `highlight`) and `message` (shown on hover for errors,
warnings and running statements). Ranges follow edits and disappear with the
text they describe. `decorations-revision` names the editor revision the
offsets were computed for; when the user typed in the meantime the editor maps
them forward through its retained edit log, and drops them only when that
revision is older than the log (the product then republishes).

### Completion

The editor raises `completionRequested(offset, trigger)` with trigger
`0` (Ctrl+Space), `1` (a word starts or continues while the list is open) or
`2` (after `.`). The product answers with `completion-items`
(`@completion.item`: `id`, `label`, `kind` badge, `detail`, `insert` — empty
means the label —, `replace_start` / `replace_end`, both 0 for "the word before
the caret") and sets `completion-open`. The list is drawn at the caret above
neighbouring widgets and filtered locally (prefix matches first, then
substring matches) while the editor asks again, so stale answers never block
typing. Up/Down/PageUp/PageDown select, Enter or Tab accept, Esc, a non-word
character or moving the caret closes. `completionClosed()` reports every close
so the product can reset `completion-open`, followed by
`completionAccepted(id)` when an item was inserted.

### Documents

One editor holds several documents and shows one, so tabs of files or queries
need a single editor. Each document keeps its text, language, undo history,
selections, scroll position and decorations while hidden; revisions stay
unique across documents. The editor starts on the document `""`.

```cpp
editor->selectDocument(L"web-01:/etc/nginx/nginx.conf"); // true: created empty
editor->setText(content);                                // load, history starts here
editor->closeDocument(id);                               // shown one -> back to ""
```

C ABI / Rust: `oneui_view_code_editor_select_document` / `View::code_editor_select_document`
(returns whether it was created), `_close_document`, `_reset` (load text and
clear history), `_execute` with a command (undo, redo, find, replace, close
find, select all, toggle comment, indent, outdent, duplicate lines, cut, copy,
paste; reports whether it applied) and `_history` (`can_undo`, `can_redo`)
for toolbars. All run outside VM callbacks.

### Styling

Colors come from the `CodeEditor` rule and `--code-*` custom properties, so
the editor follows the application theme:

```css
CodeEditor.editor { background-color: var(--surface); color: var(--text);
  caret-color: var(--accent); selection-color: #3b82f655;
  placeholder-color: var(--text-muted);          /* line numbers */
  content-background-color: #ffffff0a;           /* current line */
  border-color: var(--border); scrollbar-color: var(--scrollbar); }
:root { --code-keyword: #c678dd; --code-string: #98c379; --code-comment: #7f848e; }
```

Unset colors derive from `background-color` and `color` (gutter, current line,
indent guides, find panel, comments, operators), so a rule with only those two
gives a consistent dark or light editor. Variables: `--code-gutter-background`,
`--code-line-number`, `--code-line-number-active`, `--code-current-line`,
`--code-indent-guide`, `--code-accent`, `--code-accent-soft`, `--code-match`,
`--code-match-current`, `--code-error`, `--code-warning`, `--code-success`,
`--code-panel`, `--code-panel-border`, `--code-shadow`, and syntax colors
`--code-keyword`, `--code-type`, `--code-function`, `--code-constant`,
`--code-number`, `--code-string`, `--code-variable`, `--code-comment`,
`--code-operator`, `--code-parameter`. C++: `CodeEditor::setStyle` with
`codeEditorStyleFromStyleSheet` or `codeEditorStyleFromTheme`.

Find bar text (placeholders, "No results", the `{0} of {1}` count, replace
buttons and tooltips) uses `BuiltinText::Find*` / `Replace*`, so the
application's language applies through `setBuiltinText`.

### Keys

| Keys | Action |
| --- | --- |
| Arrows, Home/End, PageUp/PageDown, Ctrl+Home/End, Ctrl+Left/Right | Move (Shift extends); Home toggles between indentation and column 0 |
| Ctrl+Backspace / Ctrl+Delete | Delete a word |
| Enter | New line keeping the indentation |
| Tab / Shift+Tab | Indent (or insert an indent) / outdent; Ctrl+] and Ctrl+[ too |
| Ctrl+/ | Toggle line comments, aligned at the smallest indentation |
| Ctrl+D, Shift+Alt+Up/Down | Duplicate lines |
| Alt+Up/Down | Move lines |
| Ctrl+Shift+K | Delete lines |
| Alt+click, Ctrl+Alt+Up/Down | Add a cursor; Esc returns to one cursor |
| Ctrl+A / C / X / V, Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z | Clipboard and history; copy and cut without a selection take the line; pasting as many lines as cursors gives one line each |
| Ctrl+Space | Ask for completions |
| Ctrl+F / Ctrl+H, F3 / Shift+F3 | Find bar / with replace, next / previous; the bar has case, regex and whole-word toggles, match counts, Enter / Shift+Enter, Ctrl+Enter in the replace field replaces all |
| Double / triple click | Select word / line; dragging after a double click extends by words |

Any other combination (Ctrl+Enter, F5, Ctrl+Shift+E, ...) is not consumed:
command scopes see shortcuts before the focused widget, and the editor only
claims editing shortcuts, so page commands keep working while it has focus.
IME composition is drawn at the caret and committed text is one insertion.
