# Input paint before posted work

An input handler can invalidate a text field and post a more expensive model
update. The Win32 backend coalesces input drawing until an animation frame, but
the posted update can be dispatched before that timer. One long callback then
blocks even the first visible glyph.

Before executing an already extracted callback batch, the backend flushes
pending keyboard feedback. It checks window lifetime after drawing. Pointer
commands can replace the old view in that batch, so their hover and release
states stay coalesced with the scheduled frame. The original frame request
still reconciles animations and IME. The callback batch retains FIFO, its
32-callback / 4ms budget and isolation from nested message loops.

`oneui_win32_input_priority_tests` creates an owned native window and posts a
pointer release followed by a text event. It checks that the pointer command
runs before painting the old state, that text paints before the first model
callback, and that all callbacks remain ordered. The previous backend fails
the pointer ordering assertion while the keyboard and FIFO checks pass.

Run it with the existing loop, geometry and IME contracts:

```text
ninja -C build/release oneui_win32_input_priority_tests oneui_win32_window_loop_tests oneui_win32_input_layout_tests oneui_win32_ime_cancel_tests
ctest --test-dir build/release -R "oneui_win32_(input_priority|window_loop|input_layout|ime_cancel)_tests" --output-on-failure
```

The native suite checks scheduling, including flood/FIFO/nested-loop and pointer
coalescing cases. It is not an input-to-display benchmark; that requires an
independent desktop capture of the actual application and its target content.

## Input after a native message batch

The Win32 loop also dispatches messages to helper windows on the UI thread.
In foreground iShell traces, long dispatches targeted the single-threaded
COM apartment's hidden window. Separate stacks recorded UI Automation calling
MSAA while the apartment was processing these messages. [Microsoft COM STA documentation](https://learn.microsoft.com/windows/win32/com/single-threaded-apartments)

When a regular dispatch exhausts the native message batch's time or count
budget, the loop gives queued hardware input one turn. A prioritized client
mouse press can also get its matching release in the same batch, when the
release is already the next hardware input for that window. The release
uses the regular dispatch path and consumes this one continuation. Input
order and the frame-service boundary are preserved.

On Windows, a probe restricted to the input category can miss an already
observed button-up. A non-removing range probe can make it visible again.
The loop then confirms that the same release is at the front of the full
hardware queue. [Microsoft `PeekMessageW` documentation](https://learn.microsoft.com/windows/win32/api/winuser/nf-winuser-peekmessagew)

The opt-in `tests/win32_click_turn_fixture.cpp` uses a visible window and a
message-only helper on the same thread. Compile it against the selected
production DLL and run it with `click`, `twice`, `held` or `interleaved`.
It reports when each hardware event arrives and how many of five helper
messages have completed. In `click`, both edges should arrive after the
first helper. The other modes exercise the batch limit, a held button and
keyboard input between the button edges. Consumer evidence is in iShell's
`docs/native-migration/responsiveness-3x-4x.md`.

## Hover timers across input

A client press or wheel gesture dismisses the current Win32 tooltip and
cancels its pending timer. Navigation and scrolling can replace the widget
under a stationary pointer. A new pointer position or a leave and reentry can
arm another hover. Repeated move messages at the same coordinates leave the
tooltip dismissed.
Arming or discarding a hidden tooltip leaves the paint state intact. Hiding
a visible tooltip requests a redraw.

## Paint preparation and UI-thread activity

With both `ONEUI_INTERACTION_TRACE` and `ONEUI_SCROLL_TRACE` enabled, a
Win32 paint adds budget rows for backend setup, `BeginPaint`, render-target
preparation, canvas setup, widget painting and final recording. Match
`detail_a` to the interaction trace's paint sequence on the same HWND.
`duration_ms` is elapsed wall time. `detail_b` is the UI thread's cycle
delta or -1 when the cycle counter was unavailable.

The rows are written after presentation. The row timestamp is the write
time. Use the recorded durations and the interaction timestamps to locate
the measured work. CPU cycles and elapsed time together help distinguish
continued thread activity from a gap with no recorded activity. Wait and
runnable states require separate scheduler evidence.

## Live window resizing

Size messages invalidate the client area and leave painting to the normal
paint message and timer. During native sizing, the next paint delivers the
latest pending client size before preparing the canvas. The native sizing
loop can dispatch this paint even while the outer loop's waitable animation
timer is unserviced. An input event before that paint still resolves current
layout for hit testing. Ending sizing schedules a final paint for any pending
geometry.

After a live size has been painted, posted callback batches leave further
painting to the native loop for at least one display interval or the Win32
timer minimum, whichever is longer. Once that interval elapses, a queued
callback can still present pending damage to prevent a sustained callback
load from starving drawing. Native paint requests and the last size on exit
follow the regular resize path.
