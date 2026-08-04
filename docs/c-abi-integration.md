# OneUI C ABI Integration

OneUI must expose application-facing GUI features through a stable C ABI.
Remote, Go tools, Rust tools, and future products should call this ABI instead
of linking against OneUI C++ classes directly.

> 本文是 C ABI 的设计总纲（红线、形态、迁移规则）。已落地的完整函数清单、CSS 子集与组件映射见 `25-remote-oneui-css-abi-implementation.md`；实际以 `include/oneui/oneui_c_api.h` 为准。

## Red Lines

- Do not expose C++ classes, templates, STL types, exceptions, lambdas, or RTTI
  across the public ABI boundary.
- Do not build product UI with raw Win32 controls when the same capability
  should exist in OneUI.
- Do not add Remote-only visual behavior into Remote if it is a reusable UI
  primitive. Add it to OneUI first, then consume it through the C ABI.
- Do not require callers to use a C++ compiler, a matching C++ standard library,
  or a matching MSVC/MinGW version.

## Public ABI Shape

The public ABI uses:

- opaque handles, such as `OneUiWindow*` and `OneUiWidget*`
- POD option structs
- `wchar_t*` for Windows UTF-16 text
- function pointer callbacks with caller-owned `user_data`
- explicit destroy functions for caller-owned handles

This keeps the binary boundary usable from Rust, Go, C, C#, Python FFI, and
other runtimes.

## Callback Failure Boundary

Language exceptions and Rust panics must never unwind through a OneUI C ABI
callback. The safe Rust binding guards every dispatcher, command, value,
pointer, list, tree, menu, and terminal callback. Applications install a
process-wide observer with `oneui::set_callback_panic_handler`; the observer
receives a stable callback context and panic message after the panic hook has
captured the original stack.

The observer is for diagnostics only. It must not retry the callback or mutate
the control that is currently dispatching. Product applications remain
responsible for their own route or operation rollback and for preserving exact
build symbols when diagnosing native crashes.

The standard Windows build script defaults to `RelWithDebInfo`: optimized
runtime behavior is retained while `oneui.pdb` is produced for internal symbol
archives. Public or customer packages should ship the DLL, not the PDB.

## Current Surface

The initial ABI surface supports:

- window create/show/run/close
- setting a window content widget
- generic widget preferred size, disabled, visible
- `Panel` for background, border, radius, shadow, padding, and content layout
- `Stack` row/column layout with fixed and flexible children
- `Label`
- `TextField`
- `Button`
- button click callbacks
- text-field changed callbacks

## Remote Migration Rule

Remote's GUI should be implemented as:

```text
Remote Rust application
    -> dynamic load oneui.dll
    -> call OneUI C ABI
    -> OneUI owns the window, rendering, controls, input, layout, and styling
```

The old C++ Remote shell can remain temporarily as a fallback during migration,
but it should not receive new UI features.

## Next ABI Additions

The next useful additions are:

- stable style/class APIs instead of per-widget hard-coded setters
- title-bar and sidebar components
- typed layout properties such as gap, padding, alignment, flex weight
- image/icon resource APIs
- async-safe event posting into the UI thread
- text-field validation and readonly/disabled visual states
- accessibility metadata setters
