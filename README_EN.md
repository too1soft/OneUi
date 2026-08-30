# OneUI

English | [简体中文](README.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-2ea44f.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599c.svg)](CMakeLists.txt)
[![Platform](https://img.shields.io/badge/backend-Win32-0078d4.svg)](#platform-and-maturity)
[![UTF-8 ABI](https://img.shields.io/badge/UTF--8%20ABI-v16-6f42c1.svg)](include/oneui/oneui_c_api.h)
[![Version](https://img.shields.io/badge/version-0.1.0-f59e0b.svg)](CMakeLists.txt)

OneUI is a **Windows-first, native, self-drawn, retained-mode** desktop UI framework. It is
implemented in C++17, renders through Skia raster, and composes applications from a `Widget` / `View`
tree, layout containers, reactive state, and CSS-like style sheets. It does not depend on HTML, a
browser process, or WebView.

The repository currently ships:

- a public C++ API;
- a versioned UTF-8 C ABI (`ONEUI_UTF8_ABI_VERSION = 18`);
- raw `oneui-sys` Rust FFI and the safe `oneui` Rust crate;
- a Win32 window, input, DPI, clipboard, file-dialog, tray, and Skia presentation backend;
- a Gallery, a remote-component Gallery, SDK tooling, and 13 CTest behavior/contract targets.

> **Current version: 0.1.0 development release.** The Win32 path already supports real desktop
> products, but public APIs, the C ABI, and component contracts may still change during `0.x`.
> Linux and macOS contain explicit non-operational skeletons only.

## Intended Use

OneUI targets dense native tools and workspaces, including:

- SSH, terminal, operations, and remote-control clients;
- enterprise consoles, configuration tools, and internal desktop applications;
- workbenches with lists, trees, tables, split panes, overlays, and keyboard commands;
- Windows applications with a C++ framework layer and a Rust product layer;
- products that need consistent, testable UI without a browser runtime.

## Capability Overview

| Area | Implemented capabilities |
| --- | --- |
| Runtime | `Widget`, `View`, logical/physical pixels, focus chains, hit testing, keyboard/mouse/wheel input, animation frames, tooltips, accessibility metadata |
| Reactive state | `State<T>`, `Binding<T>`, window-thread dispatcher, background posting, thread-safe widget handles |
| Rendering | `Canvas`, Skia raster, text/primitives/gradients/shadows/pixel frames, clip bounds and stable viewport bounds |
| Styling | CSS-like selectors, classes, pseudo states, custom properties, typed adapters, color/opacity transitions |
| Layout | Stack, Grid, Wrap, Panel, ScrollView, SplitView, OverlayHost, ReorderableGrid, AppShell, ProductShell |
| Forms | Button, IconButton, TextField/TextArea, Checkbox, Switch, RadioGroup, Slider, Select, FormField |
| Navigation/data | Tabs, List, VirtualList, TreeView, Table, Menu, NavItem; selection, activation, context menus, reorder requests, stable drag IDs |
| Feedback/surfaces | Card, Badge, IconBadge, ProgressBar, Sparkline, Dialog, Popup, Toast, StateView, StatusStrip |
| Specialized views | TerminalView, LogView, RealtimeFrameView, RemoteInputRegion, WindowTitleBar |
| Platform services | Win32 Window, placement persistence, DPI/monitor, clipboard, file/folder pickers, confirm/prompt, tray, global raw-key |
| Diagnostics | committed widget frames, Stack content extent, interaction traces, privacy-safe widget-tree JSON snapshots |
| Interop | C++ API, 459 public C ABI function declarations, UTF-8 ABI v16, safe Rust wrappers, callback panic boundary |

See the [component inventory](docs/07-component-inventory.md) and
[component reference](docs/14-component-reference.md) for per-component coverage and limits.

## Current Workspace Additions

The current workspace adds or substantially extends:

- `Sparkline`, including C ABI and safe Rust support;
- compact, measured, icon-capable, scrollable, closable, context-aware, reorder-requesting `Tabs`;
- a structured UTF-8 `Table` with visible-row painting, smooth scrolling, single/multiple selection,
  keyboard commands, edit/delete requests, internal reorder, and external stable-ID drag;
- synchronized layout-tree JSON snapshots with parent IDs, actual/preferred frames, resolved style
  boxes, and value-safe semantic metadata;
- title-bar accessories and explicit interactive insets that avoid drag/click conflicts;
- tooltips, `InteractiveSurface` pointer-move/hover callbacks, `TextField` submission, and a
  committed `SplitView` ratio callback;
- corrected nested-overlay, out-of-bounds popup, and scrolled-Select routing;
- Rust `WidgetHandle`, `TextFieldHandle`, `SparklineHandle`, `TableHandle`, layout snapshots, and
  interaction tracing for product integration.

## Platform And Maturity

| Platform | Status | Notes |
| --- | --- | --- |
| Windows / Win32 | Only operational backend | Development, Gallery, tests, SDK, and Rust product integration all target this path |
| Windows 7 API level | Source compatibility target | CMake defines `_WIN32_WINNT=0x0601`; final OS support still depends on the selected MSVC/Skia artifacts |
| Linux | Not implemented | `Window::create` and clipboard operations throw explicit not-implemented errors |
| macOS | Not implemented | `Window::create` and clipboard operations throw explicit not-implemented errors |

Component maturity is documented as **primary**, **usable**, or **experimental**. This classification
describes current Win32 behavior, not a long-term stability guarantee.

## Architecture

```text
Application (C++ / Rust / another FFI language)
        |
        +-- public C++ API
        +-- UTF-8 C ABI v16
        +-- oneui-sys + safe oneui Rust wrappers
        |
OneUI retained widget tree
        +-- state, binding and selection model
        +-- layout, focus, hit testing and overlays
        +-- CSS-like style sheet and typed adapters
        +-- callbacks, animation and accessibility metadata
        |
Canvas abstraction
        |
Skia raster renderer
        |
Win32 backend
        +-- window/message loop/raw key/IME path
        +-- DPI/monitor/clipboard/file dialogs/tray
        +-- logical-to-physical presentation
```

Reusable behavior belongs in OneUI, not in product-specific branches. See the
[architecture guide](docs/01-architecture.md) and
[contract red lines](docs/24-oneui-contract-red-lines.md).

## Repository Layout

```text
include/oneui/        Public C++ headers and oneui_c_api.h
src/core/             Platform-neutral controls, layout, style, state, and painting
src/platform/win32/   Operational Win32 backend
src/platform/linux/   Non-wired platform skeleton
src/platform/macos/   Non-wired platform skeleton
src/capi/             C ABI and cross-boundary lifetime implementation
bindings/rust/        oneui-sys and safe oneui crate
examples/gallery/     C++ component Gallery
examples/remote_component_gallery/ Remote/product component examples
tests/                Behavior, C ABI, backend, and platform contract tests
docs/                 Current specifications, references, and design records
scripts/              Skia, build, SDK, audit, red-line, and smoke-test tooling
website/              Nuxt documentation/component-site source
```

## Building On Windows

### Prerequisites

- Git, Python 3, and PowerShell;
- CMake 3.16 or newer;
- Visual Studio or Build Tools with MSVC and the Windows SDK;
- Ninja;
- a first-time vendored Skia build.

### Recommended: MSVC + static Skia

```powershell
git clone https://github.com/too1soft/OneUi.git
cd OneUi

$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$windowsSdk = "C:/Program Files (x86)/Windows Kits/10"

.\scripts\build-skia-static.ps1 `
  -Fetch -SyncDeps -Generate -Build `
  -WinVc ($vs.Replace("\", "/") + "/VC") `
  -WinSdk $windowsSdk

.\scripts\build-oneui-msvc-bundled.ps1 `
  -VsInstall $vs `
  -Arch x64 `
  -Configuration RelWithDebInfo
```

Pass `-Proxy http://127.0.0.1:7897` to the Skia script when required.

Primary outputs:

```text
build/msvc-bundled-static/oneui.dll
build/msvc-bundled-static/oneui.lib
build/msvc-bundled-static/oneui.pdb
build/msvc-bundled-static/examples/gallery/oneui_gallery.exe
build/msvc-bundled-static/examples/remote_component_gallery/oneui_remote_component_gallery.exe
```

Run the Gallery:

```powershell
$env:PATH = "$(Resolve-Path .\build\msvc-bundled-static);$env:PATH"
& .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe
```

See [Getting Started](docs/12-getting-started.md) for MinGW/UCRT presets, SDK integration, and
troubleshooting.

## Minimal C++ Example

```cpp
#include <oneui/oneui.h>

#include <memory>

int main() {
    auto root = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
    root->setPadding(oneui::Insets{16.0f});
    root->setGap(12.0f);

    auto title = std::make_shared<oneui::Label>(L"Hello, OneUI");
    auto button = std::make_shared<oneui::Button>(L"Continue");
    button->setOnClick([] { /* application action */ });
    root->add(title);
    root->add(button);

    auto window = oneui::Window::create(L"OneUI App", 720, 480);
    window->setContent(root);
    window->show();
    return window->run();
}
```

`oneui/oneui.h` aggregates the common surface. Include advanced headers directly when needed, such
as `<oneui/controls/virtual_list.h>`, `<oneui/controls/sparkline.h>`, or
`<oneui/controls/window_title_bar.h>`.

## SDK / CMake Integration

```powershell
.\scripts\package-sdk.ps1 -Toolchain msvc-bundled-static
```

The package is written to `dist/OneUI-SDK-msvc-bundled-static.zip`. Consumer CMake:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_oneui_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
find_package(OneUI REQUIRED CONFIG)

add_executable(my_oneui_app main.cpp)
target_link_libraries(my_oneui_app PRIVATE OneUI::oneui)
```

Point `CMAKE_PREFIX_PATH` at the extracted SDK and deploy `bin/oneui.dll` next to the application.

## C ABI

[`include/oneui/oneui_c_api.h`](include/oneui/oneui_c_api.h) is the authoritative ABI definition:

- opaque handles, POD structs, and explicit destroy functions;
- length-aware `OneUiUtf8String` for new APIs;
- copied string/array inputs during calls;
- function-pointer callbacks with caller-owned `user_data`;
- runtime ABI v16 verification through `oneui_utf8_abi_version()`;
- legacy `wchar_t*` entry points retained only for existing Windows consumers.

See the [C ABI integration guide](docs/c-abi-integration.md) for lifetime, threading, callbacks, and
component mapping.

## Rust Bindings

`bindings/rust/oneui-sys` mirrors the C ABI. `bindings/rust/oneui` adds ownership, callback cleanup,
UI-thread dispatch, panic containment, and thread-safe handles.

```powershell
$env:ONEUI_LIB_DIR = (Resolve-Path .\build\msvc-bundled-static)
$env:PATH = "$env:ONEUI_LIB_DIR;$env:PATH"
cargo test --manifest-path .\bindings\rust\Cargo.toml
```

The safe layer currently covers the product-critical window, dispatcher, style, layout, overlay,
form, Tabs, List, VirtualList, TreeView, Table, TerminalView, LogView, Sparkline, and file-dialog
paths. See the [Rust bindings README](bindings/rust/README.md).

## Tests And Quality Gates

```powershell
cmake --build .\build\msvc-bundled-static
ctest --test-dir .\build\msvc-bundled-static --output-on-failure

$env:ONEUI_LIB_DIR = (Resolve-Path .\build\msvc-bundled-static)
$env:PATH = "$env:ONEUI_LIB_DIR;$env:PATH"
cargo test --manifest-path .\bindings\rust\Cargo.toml

.\scripts\check-ui-red-lines.ps1
.\scripts\audit-runtime.ps1 `
  -Binary .\build\msvc-bundled-static\oneui.dll `
  -Mode product
.\scripts\test-sdk-consumer.ps1
```

The 13 CTest targets cover controls, selection, overlays, scrolling, Stack, realtime frames, remote
input, terminal rendering, trees, Panel, the C ABI, monitor behavior, and the Win32 backend contract.

## Documentation

Start at [docs/README.md](docs/README.md). Frequently used references:

- [Architecture](docs/01-architecture.md)
- [Getting Started](docs/12-getting-started.md)
- [Component Inventory](docs/07-component-inventory.md)
- [Component Reference](docs/14-component-reference.md)
- [Style Sheets](docs/20-style-sheet.md)
- [Accessibility](docs/15-accessibility.md)
- [C ABI Integration](docs/c-abi-integration.md)
- [Rust Bindings](bindings/rust/README.md)
- [Platform Backend Contract](docs/28-platform-backend-contract.md)
- [TerminalView](docs/33-terminal-view.md)
- [TreeView](docs/34-tree-view.md)

## Known Limitations

- Win32 is the only operational backend.
- Version 0.1.0 is still converging; public APIs and ABI versions may change.
- StyleSheet is a controlled CSS-like subset, not a browser CSS engine.
- OneUI stores accessibility roles, names, descriptions, values, and states, but the Win32 UI
  Automation bridge is not complete.
- Text editing covers selection, undo/redo, paste, password, and submit; complex shaping and full IME
  scenarios still require continued validation.
- Table does not yet include built-in sorting, filtering, column resize, or an in-cell editor.
- Tabs/Table/Tree/ReorderableGrid reorder callbacks report requests; product state remains authoritative.
- RealtimeFrameView paints BGRA/RGBA pixels and supports ownership-transfer submission plus a coalescing Rust worker handle; NV12 conversion is not implemented.
- Layout JSON snapshots validate geometry/semantics but are not pixel screenshot tests.

## Contribution Rules

- Put reusable behavior in OneUI, never in product-named framework branches.
- Keep platform differences under `src/platform`.
- Define new cross-language behavior in the UTF-8 C ABI, then add safe Rust ownership.
- Add repeatable tests for input, focus, scrolling, reorder, overlay, and thread behavior.
- Express visuals through tokens, StyleSheet, and typed adapters instead of product magic numbers.
- When the ABI changes, update its version, `oneui-sys`, safe wrappers, tests, and documentation.

## License

OneUI is available under the [MIT License](LICENSE).
