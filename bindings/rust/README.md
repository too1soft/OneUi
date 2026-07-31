# OneUI Rust Bindings

`oneui-sys` maps the portable UTF-8 C ABI without adding ownership policy.
`oneui` is the safe foundation layer for Rust applications such as iShellPro.
It currently exposes owned windows plus the first composable content surface:
stacks, labels, UTF-8 text, layout padding, and content mounting. New product
code should build on these safe types instead of calling the raw C ABI.

The C++ library must be built before Rust bindings are compiled. On Windows:

```powershell
& .\scripts\build-oneui-msvc-bundled.ps1 -Arch x64
$env:ONEUI_LIB_DIR = (Resolve-Path .\build\msvc-bundled-static)
$env:PATH = "$env:ONEUI_LIB_DIR;$env:PATH"
cargo test --manifest-path .\bindings\rust\Cargo.toml
```

`ONEUI_LIB_DIR` must contain the import library and the runtime library:
`oneui.lib` plus `oneui.dll` on Windows, or `liboneui.so` on Linux. The final
application installer must ship the matching runtime library beside the Rust
application binary or register it through the platform loader path.

New Rust code must use the UTF-8 APIs. The older `wchar_t*` C functions remain
only for compatibility with existing Windows consumers.

`Window::new` initializes the hidden native handle on the calling thread. Call
it on the application UI thread, then clone `Window::dispatcher()` for
background services. `UiDispatcher::dispatch` runs `Send + 'static` work on
that UI thread and cancels queued work when the window closes.
