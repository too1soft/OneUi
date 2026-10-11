# Declarative resource picker contract

VirtualList supports `multiple: bool` (default single selection) and optional `selected: bool` in each `items` row. Existing rows without `selected` preserve prior selection behavior. Item identity and hidden selections remain application model concerns.

`selectionChanged { index: i64, selected: bool }` emits one event per changed visible row, including deselections. Programmatic item/selection updates synchronize the event baseline without echoing commands. The view model must map visible indexes to domain identities before filtering.

`tests/workspace/Pickers.one` and `picker_runtime_tests.cpp` cover initial multiple selection, changed-only events, and no programmatic echo. The native iShell resource picker additionally covers filtered hidden choices, submit and cancel.

Popup style capability advertises background, color, borders, radius, padding, gap and shadow, matching the existing supported runtime style properties. The change is identical to tools lane commit 90c256 and must be retained only once on integration. `globe` is a public icon alias for the existing Globe symbol.
