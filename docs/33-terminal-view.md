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
- per-cell text, foreground, background, style flags, and an optional stable
  hyperlink identifier;
- a cursor position and visibility state.

The component copies incoming cells. Callers can reuse or drop their source
frame after `setGrid` / `set_grid` returns. A malformed or incomplete grid is
normalized to blank cells; it never becomes a delimiter-encoded string or an
unbounded allocation.

Terminal text uses the `Canvas` monospace-family path for both glyph metrics
and painting. On Windows, OneUI selects `JetBrains Mono`, `Cascadia Mono`,
`Consolas`, then `NSimSun` when available, before falling back to the default
system face. This keeps renderer cell geometry stable without changing fonts
used by the rest of the application.

`TerminalView` deliberately does not include a PTY, SSH client, shell selection,
escape-sequence parser, scrollback store, or key encoder. Those are session
engine concerns. The first iShellPro integration uses `vt100` as the parser and
maps its frame into this public component contract.

OSC 8 URI strings remain owned by the terminal engine. Cells carry only a
bounded integer identifier, and `setOnHyperlink` / `set_on_hyperlink` reports
that identifier after a completed Ctrl+left-click. OneUI adds hover underline
and pointer feedback, but it never resolves or opens a URI. The product must
resolve the identifier and apply its own protocol and trust policy.

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
terminal->setOnHyperlink([&](std::uint32_t hyperlinkId) {
    session.activateHyperlink(hyperlinkId);
});
```

Terminal key handling is intentionally split into text input and raw key input.
Text input is suitable for IME and printable text. Raw key input preserves
virtual-key, scan-code, modifier, repeat, and key-up/down information for the
terminal session's key encoder. A product must avoid writing both events for
the same keystroke.

`setOnViewportChanged` / `set_on_viewport_changed` reports the integer row and
column capacity calculated from the view's actual native canvas and monospace
font metrics. Session controllers should resize their PTY or remote terminal
only after this callback, rather than duplicating font or pixel calculations.

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
`TerminalCursor`, `TerminalFrame`, `TerminalColor`, `RawKeyEvent`, and
`terminal_style`. Callbacks are owned by the Rust view, cleared before
destruction, and protected from panics crossing the C ABI.

`TerminalView` is a UI-thread object. A session worker must obtain a
`TerminalViewHandle` from `Window::terminal_view_handle(&terminal)` and call
`submit_frame(frame)`. The handle keeps only the newest pending frame and posts
one UI task at a time, so high-volume output does not create an unbounded queue.
When the terminal view is dropped, pending data is discarded and future submits
return `Error::WidgetDestroyed` without dereferencing the native widget.

## Validation

The component has C++ behavior coverage for grid normalization, wide cells,
cursor state, separated input callbacks, hyperlink hover, and explicit
Ctrl-click activation. The C ABI test covers structured UTF-8 cells and callback
registration. Rust binding tests cover hyperlink identifiers in frame diffs and
across the C ABI, and mount a native terminal view in a native window.
