# OneUI Overlay Fill and Input E2E Case

Date: 2026-05-29

## Case

Remote's packaged OneUI GUI passed registration, temporary-code creation,
session creation, and first-frame display, but the real session-window click did
not reach the runtime input writer. The missing behavior was not Remote-specific:
OneUI needed a reusable way for an overlay to fill its host for hit testing.

## OneUI Change

`OverlayHost::addAnchoredOverlay` now supports negative width or height as a
generic fill contract:

- `size.width < 0`: fill the available host width after margins.
- `size.height < 0`: fill the available host height after margins.
- `size.width > 0` or `size.height > 0`: keep explicit size.
- `size.width == 0` or `size.height == 0`: keep preferred-size behavior.
- Non-anchored overlays remain caller-positioned.

This gives products a general way to create full-surface transparent overlays
for input, drag areas, glass panes, and modal interaction layers without writing
product-specific coordinate logic.

## Red Line

Do not solve full-surface overlay needs with hardcoded product coordinates,
magic widths, component names, page names, or downstream window hooks. If an
overlay participates in hit testing, its layout contract must live in OneUI and
must have behavior tests.

## Tests

The behavior is covered by:

```text
E:\project\byname\oneui\tests\overlay_host_behavior_tests.cpp
```

The regression is covered downstream by:

```text
E:\project\byname\remote\scripts\test-oneui-gui-e2e-loopback.ps1
```

The downstream E2E must continue clicking the real session window and waiting
for both controller-side input telemetry and host-side input arrival/injection
telemetry. Direct runtime-only tests are not enough for this class of bug.

The E2E must include keyboard input as well as pointer input. Remote now sends a
visible session-window virtual-key tap and requires both key down and key up
frames to be observed on the controller and injected on the host. It also
asserts exactly one down and one up frame for one tap, so duplicate keyboard
emission cannot silently return.

The downstream E2E must also reject zero-coordinate mouse move frames emitted
before remote content geometry is known. Unknown geometry must be a not-ready
state in `RemoteInputRegion`, not a fake `(0, 0)` coordinate.

## Downstream Runtime Lesson

The downstream Remote shell originally forwarded session-window input through a
localhost TCP bridge even though the shell and runtime core were already in the
same Rust process. That extra bridge hid failures and added an avoidable
latency/failure point. The shell now forwards input directly through the runtime
worker into `remote-runtime-core::send_controller_input`.

The controlled host's input relay reader also has to be isolated from screen
capture and encoding. A blocking capture loop can starve an async task scheduled
on the same runtime. Remote now runs the host input relay reader on a dedicated
OS thread with its own Tokio runtime.

OneUI implication: input overlays and input-capable widgets should expose clean,
generic hit-testing and event-delivery semantics, but product runtimes must keep
their low-latency input transport independent from heavy video work.

Related red-line note:

```text
E:\project\byname\oneui\docs\31-runtime-input-red-lines.md
```
