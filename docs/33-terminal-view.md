# TerminalView

`TerminalView` is OneUI's native terminal presentation component. It renders a
structured terminal frame without embedding a browser, parsing escape sequences,
or owning a process. Keeping these responsibilities separate lets a product use
the same component for local PTYs, SSH, serial sessions, replay, and tests.

## Contract

The product-side terminal engine owns terminal semantics and passes a complete
frame to the view:

- grid rows and columns;
- one `TerminalCell` per grid position;
- per-cell text, foreground, background, and style flags;
- a cursor position and visibility state.

The component copies incoming cells. Callers can reuse or drop their source
frame after `setGrid` / `set_grid` returns. A malformed or incomplete grid is
normalized to blank cells; it never becomes a delimiter-encoded string or an
unbounded allocation.

`TerminalView` deliberately does not include a PTY, SSH client, shell selection,
escape-sequence parser, scrollback store, or key encoder. Those are session
engine concerns. The first iShellPro integration uses `vt100` as the parser and
maps its frame into this public component contract.

## C++ API

```cpp
auto terminal = std::make_shared<oneui::TerminalView>();
terminal->setFontSize(13.0f);
terminal->setPalette(
    oneui::Color{20, 24, 36, 255},
    oneui::Color{220, 226, 240, 255},
    oneui::Color{170, 190, 255, 255});
terminal->setGrid(rows, columns, cells);
terminal->setCursor(oneui::TerminalCursor{row, column, true});
terminal->setOnTextInput([&](const std::wstring& text) {
    session.writeUtf8(text);
});
```

Terminal key handling is intentionally split into text input and raw key input.
Text input is suitable for IME and printable text. Raw key input preserves
virtual-key, scan-code, modifier, repeat, and key-up/down information for the
terminal session's key encoder. A product must avoid writing both events for
the same keystroke.

## Style Flags

The ABI-stable style bits are:

- `TerminalCellStyleBold`
- `TerminalCellStyleDim`
- `TerminalCellStyleItalic`
- `TerminalCellStyleUnderline`
- `TerminalCellStyleInverse`
- `TerminalCellStyleWide`
- `TerminalCellStyleWideContinuation`

Wide continuation cells reserve layout space for the preceding wide character
and are not independently painted.

## Rust API

The `oneui` safe binding exposes `TerminalView`, `TerminalCell`,
`TerminalCursor`, `TerminalColor`, `RawKeyEvent`, and `terminal_style`.
Callbacks are owned by the Rust view, cleared before destruction, and protected
from panics crossing the C ABI. `TerminalView` is a UI-thread object; session
workers must dispatch frame updates to the window UI dispatcher.

## Validation

The component has C++ behavior coverage for grid normalization, wide cells,
cursor state, and separated input callbacks. The C ABI test covers structured
UTF-8 cells and callback registration. The Rust binding test creates the native
view, sets a wide-character frame and cursor, and mounts it in a native window.
