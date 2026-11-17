# Terminal selection and host shortcuts

TerminalView preserves selection during modified key chords before invoking
the host raw-key callback. This lets a host implement smart Ctrl+C: copy when
a selection exists, send ETX otherwise. Host Select All can create a selection
without the same key event clearing it afterward. Plain keys, committed text
and paste retain their normal selection-clearing behavior. The built-in
platform copy/paste shortcuts continue to use the configured clipboard.

Dragging beyond a vertical edge scrolls one row per acknowledged frame, with
a 50ms interval. Wheel scrolling during a drag keeps the anchor and limits
steps to overlapping viewports. Release or interaction cancellation stops the
animation. The host must publish scrollback metadata before replacing grid
cells, as the Rust frame adapter does.

Selection coordinates can extend outside the visible grid. Before a viewport
change, the view remembers its rows as cell text, retaining wide-character
continuations. Painting clips the range to the viewport; copying combines
visible and remembered rows. New selection, grid resizing and history clearing
release remembered text. A jump larger than a viewport clears selection rather
than inventing text for rows the view has never received. This is local
selection; remote mouse reporting retains ownership unless the user requests
local selection with Shift.

Regression targets are `oneui_terminal_selection_scroll_tests`,
`oneui_terminal_view_behavior_tests`, and `oneui_terminal_raster_tests`.
The C ABI is unchanged; rebuild C++ consumers with matching headers and runtime.
