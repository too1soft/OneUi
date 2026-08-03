# OneUI

English | [简体中文](README.md)

[![License: MIT](https://img.shields.io/badge/License-MIT-2ea44f.svg)](LICENSE)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599c.svg)](CMakeLists.txt)
[![Platform](https://img.shields.io/badge/platform-Windows-0078d4.svg)](#platform-status)
[![Status](https://img.shields.io/badge/status-early%20development-f59e0b.svg)](#project-status)

OneUI is a native, self-drawn UI framework for desktop applications. It is built with C++17,
uses Skia raster as its current rendering foundation, and organizes interfaces through a retained
widget tree, layouts, reactive state, and CSS-like style sheets. It does not depend on a browser or
WebView.

The project is currently Windows-first. Its goal is to provide a stable, compact, and testable GUI
foundation for tools, operations clients, and enterprise desktop applications. OneUI exposes a C++
API, a stable UTF-8 C ABI, and safe Rust bindings.

> **Project status: early development.** The Win32 backend, Gallery, component library, and MSVC
> SDK are operational, but APIs may still change. Linux and macOS currently contain platform
> skeletons only and are not ready for production applications.

## Why OneUI

- **A genuinely native desktop runtime:** no HTML, JavaScript, browser process, or WebView.
- **Consistent self-drawn controls:** OneUI owns control behavior and visuals instead of inheriting
  platform theme differences.
- **A clear platform boundary:** core controls are separated from Win32 windows, input, DPI, IME,
  clipboard, and presentation.
- **Built for complex tools:** product shells, virtual lists, trees, tables, terminals, and realtime
  frame surfaces are first-class use cases.
- **Cross-language integration:** use the C++ API directly or integrate Rust through the UTF-8 C ABI
  and safe wrapper.
- **A distributable SDK:** the MSVC product build uses the static MSVC runtime and vendored static
  Skia. End users do not need to install MSYS2, Skia, or the Visual C++ Runtime separately.

## Project Status

OneUI has moved beyond its original window-and-button prototype. The repository currently includes:

| Area | Current capabilities |
| --- | --- |
| Foundation | `Widget`, `View`, `Canvas`, animation, `State<T>` / `Binding<T>` |
| Layout | Stack, Grid, Wrap, Dock, Split, Scroll, Panel, Overlay, App/Product Shell |
| Forms | Button, IconButton, TextField, Checkbox, Switch, RadioGroup, Slider, Select, FormField |
| Navigation and data | Tabs, List, VirtualList, TreeView, Table, Menu, NavItem |
| Feedback and surfaces | Card, Badge, ProgressBar, Dialog, Popup, Toast, StateView, StatusStrip |
| Specialized views | TerminalView, LogView, RealtimeFrameView, RemoteInputRegion |
| Platform and interop | Win32 window, DPI/monitor, clipboard, IME path, UTF-8 C ABI, Rust bindings |

Components do not all have the same stability level. Before adopting one in a product, review the
[component reference](docs/14-component-reference.md) and
[component inventory](docs/07-component-inventory.md).

## Platform Status

| Platform | Status | Notes |
| --- | --- | --- |
| Windows / Win32 | Operational primary backend | Current development, testing, Gallery, and SDK platform |
| Windows 7+ | Compatibility target | APIs and dependencies are selected with compatibility in mind; real-system validation is ongoing |
| Linux | Platform skeleton | The X11/Wayland backend is not operational yet |
| macOS | Platform skeleton | The Cocoa backend is not operational yet |

## Architecture

```text
Application (C++ or Rust)
        |
        +-- C++ API / UTF-8 C ABI / safe Rust bindings
        |
OneUI retained widget tree
        +-- state and binding
        +-- layout and focus
        +-- CSS-like style sheet and typed adapters
        +-- input dispatch and accessibility semantics
        |
Canvas abstraction
        |
Skia raster renderer
        |
Platform backend (Win32 today)
```

Core code must not accumulate product-specific branches. Reusable components belong in OneUI and
must include behavior tests and a stable cross-language boundary. See the
[architecture guide](docs/01-architecture.md) and
[OneUI contract red lines](docs/24-oneui-contract-red-lines.md).

## Repository Layout

```text
include/oneui/        Public C++ API
src/core/             Cross-platform controls, layouts, styles, and state
src/platform/         Platform backends; Win32 is implemented today
src/capi/             UTF-8 C ABI
bindings/rust/        oneui-sys and safe Rust wrappers
examples/gallery/     Component Gallery
tests/                Control, layout, terminal, C ABI, and backend contract tests
docs/                 Design, component, integration, and migration documentation
scripts/              Skia, OneUI, SDK, audit, and test scripts
```

## Building On Windows

### Prerequisites

- Git, Python 3, and PowerShell
- CMake 3.16 or newer
- Visual Studio or Build Tools with the MSVC C++ toolchain and Windows SDK
- Ninja, normally included with Visual Studio

The first build downloads and compiles Skia. Adjust the following paths for your Visual Studio and
Windows SDK installations:

```powershell
git clone https://github.com/too1soft/OneUi.git
cd OneUi

$vs = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$windowsSdk = "C:/Program Files (x86)/Windows Kits/10"
$env:PATH = "$vs\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;$env:PATH"

.\scripts\build-skia-static.ps1 `
  -Fetch -SyncDeps -Generate -Build `
  -WinVc ($vs.Replace("\", "/") + "/VC") `
  -WinSdk $windowsSdk

.\scripts\build-oneui-msvc-bundled.ps1 -VsInstall $vs
```

If your network requires a proxy, add the following option to the Skia script:

```powershell
-Proxy http://127.0.0.1:7897
```

Build outputs:

```text
build/msvc-bundled-static/oneui.dll
build/msvc-bundled-static/oneui.lib
build/msvc-bundled-static/examples/gallery/oneui_gallery.exe
```

Run the Gallery:

```powershell
$env:PATH = "$(Resolve-Path .\build\msvc-bundled-static);$env:PATH"
& .\build\msvc-bundled-static\examples\gallery\oneui_gallery.exe
```

## Minimal C++ Example

```cpp
#include <oneui/oneui.h>

#include <memory>

int main() {
    auto root = std::make_shared<oneui::Stack>(oneui::StackDirection::Column);
    root->setGap(12.0f);
    root->setPadding(oneui::Insets{16.0f});

    auto title = std::make_shared<oneui::Label>(L"Hello, OneUI");
    auto button = std::make_shared<oneui::Button>(L"Continue");
    root->add(title);
    root->add(button);

    auto window = oneui::Window::create(L"OneUI App", 720, 480);
    window->setContent(root);
    window->show();
    return window->run();
}
```

Use [`examples/gallery`](examples/gallery) as the authoritative reference for window creation and
component composition.

## SDK And CMake Integration

Create a distributable SDK:

```powershell
.\scripts\package-sdk.ps1 -Toolchain msvc-bundled-static
```

The package is written to:

```text
dist/OneUI-SDK-msvc-bundled-static.zip
```

Consumer CMake configuration:

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_oneui_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
find_package(OneUI REQUIRED)

add_executable(my_oneui_app main.cpp)
target_link_libraries(my_oneui_app PRIVATE OneUI::oneui)
```

Point `CMAKE_PREFIX_PATH` at the extracted SDK root and place `bin/oneui.dll` next to the application
executable. See the [getting started guide](docs/12-getting-started.md) for details.

## Rust Bindings

`bindings/rust/oneui-sys` exposes the raw UTF-8 C ABI. `bindings/rust/oneui` provides safe wrappers
with explicit ownership. Build the C++ library before running the Rust tests:

```powershell
$env:ONEUI_LIB_DIR = (Resolve-Path .\build\msvc-bundled-static)
$env:PATH = "$env:ONEUI_LIB_DIR;$env:PATH"
cargo test --manifest-path .\bindings\rust\Cargo.toml
```

New Rust code should use the UTF-8 API. The legacy `wchar_t*` ABI exists only for compatibility with
existing Windows consumers.

## Tests And Quality Checks

```powershell
ctest --test-dir .\build\msvc-bundled-static --output-on-failure

.\scripts\audit-runtime.ps1 `
  -Binary .\build\msvc-bundled-static\oneui.dll `
  -Mode product

.\scripts\check-ui-red-lines.ps1
.\scripts\test-sdk-consumer.ps1
```

Behavior tests cover control state, layout, overlays, scrolling, trees, terminal rendering, realtime
frames, the C ABI, DPI, and the Win32 backend contract. Behavioral changes must include matching tests.

## Current Limitations

- Linux and macOS backends are not operational yet.
- Public APIs and the ABI are still converging during the `0.x` phase.
- Complex text shaping, complete IME support, platform accessibility bridges, and visual snapshots
  still need work.
- Some advanced controls provide an MVP but do not yet carry a stable-release interaction or
  accessibility guarantee.
- OneUI is not a browser CSS implementation. StyleSheet provides CSS-like selectors/states and typed
  adapters.

## Roadmap

Near-term priorities:

1. Stabilize the Win32 backend, text input, DPI, focus, accessibility, and SDK contracts.
2. Continue hardening virtualized data components and performance-critical views such as TerminalView.
3. Build an operational Linux backend and run it through the same platform contract suite.
4. Publish the first public release after the ABI and component behavior stabilize.

See the complete [roadmap](docs/02-roadmap.md).

## Contributing

Issues, design discussions, and pull requests are welcome. Before contributing code, follow these rules:

- Reusable capabilities belong in OneUI. Do not add product names or product-specific branches to the
  framework.
- Express styling through tokens, StyleSheet, and typed adapters instead of accumulating hard-coded
  values inside product controls.
- Keep platform differences such as input, DPI, focus, IME, and clipboard behind the platform boundary.
- C ABI strings use length-delimited UTF-8, with explicit ownership, thread, and callback lifetime rules.
- Behavioral changes require tests. Performance-sensitive lists and realtime views require repeatable
  performance validation.

Recommended reading: [design language](docs/06-design-language.md),
[component authoring guide](docs/13-authoring-guide.md), and
[downstream product integration standard](docs/17-downstream-product-integration-standard.md).

## License

OneUI is available under the [MIT License](LICENSE).
