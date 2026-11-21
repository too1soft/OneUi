# Input paint before posted work

An input handler can invalidate a text field and post a more expensive model
update. The Win32 backend coalesces input drawing until an animation frame, but
the posted update can be dispatched before that timer. One long callback then
blocks even the first visible glyph.

Before executing an already extracted callback batch, the backend flushes any
pending input paint. It checks window lifetime after drawing. The original frame
request still reconciles animations and IME; the callback batch keeps its FIFO,
existing 32-callback / 4ms budget and isolation from nested message loops. This
does not add a delay, a busy loop, a queue thread or a change to focus policy.

`oneui_win32_input_priority_tests` creates an owned native window, posts a text
event and queues model work from its handler. It checks that input paint precedes
the first callback and that all callbacks remain ordered. The previous backend
fails the paint-order assertion while its FIFO and loop-result checks pass.

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
budget, the loop gives queued hardware input one turn. The next dequeue
uses the ordinary Windows order. A message already dequeued on the priority
turn cannot renew it. The current handler finishes before this check, and
the existing frame sources are serviced at the same batch boundary.

For verification, use a visible application window and a message-only helper
on the same thread. Queue multiple timed helper messages and send hardware
mouse input during the first handler. Record both button edges and check that
all helper messages complete in their original order. Consumer evidence is
in iShell's `docs/native-migration/responsiveness-3x-4x.md`.

## Hover timers across input

A client press or wheel gesture dismisses the current Win32 tooltip and
cancels its pending timer. Navigation and scrolling can replace the widget
under a stationary pointer. A new pointer position or a leave and reentry can
arm another hover. Repeated move messages at the same coordinates leave the
tooltip dismissed.
Arming or discarding a hidden tooltip leaves the paint state intact. Hiding
a visible tooltip requests a redraw.
